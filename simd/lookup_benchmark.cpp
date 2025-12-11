#include <benchmark/benchmark.h>
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <cassert>
/// For test.
#include <array>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>
#include <execution>

namespace stdr = std::ranges;
namespace stdv = std::views;

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            self.template operator()<I + 1>();
        }
    }.template operator()<First>();
};

template <size_t Lane>
struct simdify_t: std::ranges::range_adaptor_closure<simdify_t<Lane>> {
    constexpr auto operator()(auto &&r) const noexcept {
        auto v = std::forward<decltype(r)>(r) | std::views::all;
        auto n = std::ranges::size(v) / Lane;
        auto i = std::views::iota(size_t{0}, n);
        auto f = [v](auto index) -> decltype(auto) {
            return v[index * Lane];
        };
        return std::views::transform(i, f);
    }
};
template <auto i>
constexpr simdify_t<i> simdify;

template <size_t ILP = 4>
void lookup_ilp(stdr::range auto &&source, stdr::range auto &&lookup_table) {
    assert(stdr::size(lookup_table) >= 256 && "We need a char-width table.");
    constexpr auto table_size = 256;
    constexpr auto lane = sizeof(__m256i);
    constexpr auto bulk = ILP * lane;

    __m256i luts[table_size / sizeof(__m128i)];
    constexpr_for<0, stdr::size(luts)>([&, data = stdr::data(lookup_table)]<auto Index> {
        auto buffer128 = _mm_loadu_si128((__m128i*)(data) + Index);
        luts[Index] = _mm256_broadcastsi128_si256(buffer128);
    });

    const auto simd_zero = _mm256_setzero_si256();
    auto reduce = [&](__m256i *simd_addr) {
        // Reused in each reduction.
        // The first round of reduction uses size(luts)/2.
        // The second round uses size(luts)/4, size(luts)/8, ...
        __m256i arenas[stdr::size(luts) / 2];
        auto full = _mm256_loadu_si256(simd_addr);
        auto nibble = _mm256_and_si256(full, _mm256_set1_epi8((char)0x0f));

        // Start from bit 4 to bit 7.
        constexpr auto bias = 4;
        constexpr auto for_each_bit = constexpr_for<4, 8>;
        for_each_bit([&]<auto Bit> {
            constexpr auto bit_shift = static_cast<char>(1 << Bit);
            auto test1 = _mm256_and_si256(full, _mm256_set1_epi8(bit_shift));
            auto blend_mask = _mm256_cmpeq_epi8(test1, simd_zero);

            // Bit 4 as the first round.
            constexpr auto round = Bit - bias;
            constexpr auto tree_depth = stdr::size(arenas) >> round;
            constexpr auto tree_reduce = constexpr_for<0, tree_depth>;
            tree_reduce([&]<auto Level> {
                auto emit = [&]<auto i> /* -> decltype(auto) */ {
                    if constexpr (round > 0) return arenas[i];
                    else return _mm256_shuffle_epi8(luts[i], nibble);
                };
                auto &&lo = emit.template operator()<Level * 2>();
                auto &&hi = emit.template operator()<Level * 2 + 1>();
                arenas[Level] = _mm256_blendv_epi8(hi, lo, blend_mask);
            });
        });
        _mm256_storeu_si256(simd_addr, arenas[0]);
    };

    auto process_simd = [&](auto konstexpr_for, auto simd_view) {
        for(auto &&simd_v : simd_view) {
            konstexpr_for([&, addr = &simd_v]<auto Index> {
                reduce((__m256i*)(addr) + Index);
            });
        }
    };

    auto bulk_simd_view = source | simdify<lane> | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    auto single_simd_view = source
                          | stdv::drop(bulk * stdr::size(bulk_simd_view))
                          | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = source
                     | stdv::drop(bulk * stdr::size(bulk_simd_view))
                     | stdv::drop(lane * stdr::size(single_simd_view));
    for(auto &v : scalar_view) {
        v = lookup_table[v];
    }
}

void lookup_avx2(stdr::range auto &&source, stdr::range auto &&lookup_table) {
    assert(stdr::size(lookup_table) >= 256 && "We need a char-width table.");
    constexpr auto table_size = 256;
    constexpr auto lane = sizeof(__m256i);
    __m256i luts[table_size / sizeof(__m128i)];
    constexpr_for<0, stdr::size(luts)>([&, data = stdr::data(lookup_table)]<auto Index> {
        auto buffer128 = _mm_loadu_si128((__m128i*)(data) + Index);
        luts[Index] = _mm256_broadcastsi128_si256(buffer128);
    });

    // Reused in each reduction.
    // The first round of reduction uses size(luts)/2.
    // The second round uses size(luts)/4, size(luts)/8, ...
    __m256i arenas[stdr::size(luts) / 2];

    auto simd_zero = _mm256_setzero_si256();
    auto simd_view = source | simdify<lane>;
    for(auto &&simd_v : simd_view) {
        auto addr = &simd_v;
        auto full = _mm256_loadu_si256((__m256i*)addr);
        auto nibble = _mm256_and_si256(full, _mm256_set1_epi8((char)0x0f));

        // Start from bit 4 to bit 7.
        constexpr auto bias = 4;
        constexpr auto for_each_bit = constexpr_for<4, 8>;
        for_each_bit([&]<auto Bit> {
            constexpr auto bit_shift = static_cast<char>(1 << Bit);
            auto test1 = _mm256_and_si256(full, _mm256_set1_epi8(bit_shift));
            auto blend_mask = _mm256_cmpeq_epi8(test1, simd_zero);

            // Bit 4 as the first round.
            constexpr auto round = Bit - bias;
            constexpr auto tree_depth = stdr::size(arenas) >> round;
            constexpr auto tree_reduce = constexpr_for<0, tree_depth>;
            tree_reduce([&]<auto Level> {
                auto emit = [&]<auto i> /* -> decltype(auto) */ {
                    if constexpr (round > 0) return arenas[i];
                    else return _mm256_shuffle_epi8(luts[i], nibble);
                };
                auto &&lo = emit.template operator()<Level * 2>();
                auto &&hi = emit.template operator()<Level * 2 + 1>();
                arenas[Level] = _mm256_blendv_epi8(hi, lo, blend_mask);
            });
        });
        _mm256_storeu_si256((__m256i*)addr, arenas[0]);
    }
    auto scalar_view = source
                     | stdv::drop(lane * stdr::size(simd_view));
    for(auto &v : scalar_view) {
        v = lookup_table[v];
    }
}

void lookup_scalar(auto &&rng, auto &&lut) {
    for(auto &v : rng) {
        v = lut[v];
    }
}

void lookup_std_transform(auto &&rng, auto &&lut) {
    std::transform(stdr::begin(rng), stdr::end(rng), stdr::begin(rng),
        [&](auto idx) { return lut[idx]; });
}

void lookup_std_execution(auto &&rng, auto &&lut) {
    std::transform(std::execution::unseq,
        stdr::begin(rng), stdr::end(rng), stdr::begin(rng),
        [&](auto idx) { return lut[idx]; });
}

void lookup_nop(auto&&,auto&&){}

// ----------------------------------------------------------------------------
// Google Benchmark Infrastructure
// ----------------------------------------------------------------------------

// Generate constant random LUT (256 bytes)
const auto& get_lut() {
    static const auto arr = [] {
        std::array<uint8_t, 256> result;
        std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 255);
        stdr::generate(result, [&] { return static_cast<uint8_t>(dist(gen)); });
        return result;
    }();
    return arr;
}

// Generate random source data
template <size_t Size>
const auto& generate_data() {
    static const auto arr = [] {
        std::array<uint8_t, Size> result;
        std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, 255);
        stdr::generate(result, [&] { return static_cast<uint8_t>(dist(gen)); });
        return result;
    }();
    return arr;
}

template <size_t Size>
void BM_run(benchmark::State& state, auto &&func) {
    // 1. Get static const data (no cost here)
    const auto& source_template = generate_data<Size>();
    const auto& lut = get_lut();

    for(auto _ : state) {
        // 2. Make a mutable copy on the stack
        // Note: This copy overhead is included in the benchmark time.
        // Since lookup is O(N) and copy is O(N), this dilutes the speedup ratio
        // but keeps the code structure simple as requested.
        auto rng = source_template;

        // 3. Run algo
        func(rng, lut);

        // 4. Prevent optimization
        benchmark::DoNotOptimize(rng);
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size) * sizeof(uint8_t));
}

template <size_t ...Is>
void register_tests(std::integer_sequence<size_t, Is...>,
                    auto name, auto func) {
    (benchmark::RegisterBenchmark(
        std::string(name) + "/" + std::to_string(Is),
        [func](benchmark::State& state) {
            BM_run<Is>(state, func);
        }
    ), ...);
}

int main(int argc, char** argv) {
    std::integer_sequence<size_t,
        35,
        350,
        3502,
        35023,
        350234
    > seq;

    auto register_test = [seq](auto name, auto func) {
        register_tests(seq, name, func);
    };

    register_test("BM_nop",
        [](auto &r, auto &lut) { lookup_nop(r, lut); });

    register_test("BM_lookup_avx2",
        [](auto &r, auto &lut) { lookup_avx2(r, lut); });

    register_test("BM_lookup_ilp<1>",
        [](auto &r, auto &lut) { lookup_ilp<1>(r, lut); });

    register_test("BM_lookup_ilp<2>",
        [](auto &r, auto &lut) { lookup_ilp<2>(r, lut); });

    register_test("BM_lookup_ilp<4>",
        [](auto &r, auto &lut) { lookup_ilp<4>(r, lut); });

    register_test("BM_lookup_ilp<6>",
        [](auto &r, auto &lut) { lookup_ilp<6>(r, lut); });

    register_test("BM_lookup_ilp<8>",
        [](auto &r, auto &lut) { lookup_ilp<8>(r, lut); });

    register_test("BM_lookup_scalar",
        [](auto &r, auto &lut) { lookup_scalar(r, lut); });

    register_test("BM_std_transform",
        [](auto &r, auto &lut) { lookup_std_transform(r, lut); });

    register_test("BM_std_execution",
        [](auto &r, auto &lut) { lookup_std_execution(r, lut); });

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
