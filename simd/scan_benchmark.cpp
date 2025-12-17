#include <benchmark/benchmark.h>
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
/// For test.
#include <array>
#include <random>
#include <tuple>
#include <numeric>
#include <execution>

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    static_assert(Last - First >= 0);
    return [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            return self.template operator()<I + 1>();
        } else {
            return Last - First;
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
int scan_ilp(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    constexpr auto bulk = ILP * lane;
    auto inner_scan = [](const __m256i &v1) {
        // X2:
        // h g f e | d c b a (from v)
        // g f e 0 | c b a 0 (from s)
        // =>
        //  ...    | c+d,b+c,a+b,a
        auto s1 = _mm256_slli_si256(v1, sizeof(int));
        auto v2 = _mm256_add_epi32(v1, s1);

        // X4:
        //  ...    | c+d,b+c,a+b,a
        //  ...    | a+b,a
        // =>
        //  ...    | a+b+c+d,a+b+c,a+b,a
        auto s2 = _mm256_slli_si256(v2, 2 * sizeof(int));
        auto v3 = _mm256_add_epi32(v2, s2);

        // X8:
        // Split:
        // lo = [a+b+c+d,a+b+c,a+b,a]
        // hi = [e+f+g+h,e+f+g,e+f,e]
        auto lo = _mm256_castsi256_si128(v3);
        auto hi = _mm256_extracti128_si256(v3, 1);
        // Use MM128.
        // lo_fixed = [a+b+c+d,a+b+c+d,a+b+c+d,a+b+c+d]
        auto lo_fixed = _mm_shuffle_epi32(lo, _MM_SHUFFLE(3,3,3,3));
        auto hi_fixed = _mm_add_epi32(hi, lo_fixed);
        return _mm256_set_m128i(hi_fixed, lo);

#if 0 // Full MM256 but slower.
        // X8:
        // Cross lane:
        // a+b+c+d,a+b+c,a+b+c+d,a+b+c | a+b+c+d,a+b+c,a+b+c+d,a+b+c
        auto p3 = _mm256_permute4x64_epi64(v3, _MM_SHUFFLE(1, 1, 1, 1));
        // Broadcast:
        // a+b+c+d,a+b+c+d,a+b+c+d,a+b+c+d | a+b+c+d,a+b+c+d,a+b+c+d,a+b+c+d
        auto s3 = _mm256_shuffle_epi32(p3, _MM_SHUFFLE(3, 3, 3, 3));
        // a+b+c+d+e+f+g+h,a+b+c+d+e+f+g,a+b+c+d+e+f,a+b+c+d+e | XXXX
        auto a3 = _mm256_add_epi32(v3, s3);

        // What we need.
        return _mm256_blend_epi32(v3, a3, 0b11110000);
#endif
    };
    auto get_carry = [](const __m256i &result) {
        auto buffer = _mm256_permute4x64_epi64(result, _MM_SHUFFLE(3, 3, 3, 3));
        return _mm256_shuffle_epi32(buffer, _MM_SHUFFLE(1, 1, 1, 1));
    };
    auto sum = _mm256_setzero_si256();

    auto process_simd = [&](auto static_for, auto simd_view) {
        constexpr auto size = static_for([]<auto>{});
        __m256i results[size] {};
        __m256i carries[size] {};
        for(auto &&simd_v : simd_view) {
            static_for([&, addr = &simd_v]<auto Index> {
                auto loadu = _mm256_loadu_si256((__m256i*)(addr + Index * lane));
                results[Index] = inner_scan(loadu);
            });
            static_for([&]<auto Index> {
                carries[Index] = get_carry(results[Index]);
            });
            static_for([&, addr = &simd_v]<auto Index> {
                auto result = _mm256_add_epi32(results[Index], sum);
                _mm256_storeu_si256((__m256i*)(addr + Index * lane), result);
                sum = _mm256_add_epi32(sum, carries[Index]);
            });
        }
    };

    auto bulk_simd_view = rng
                        | simdify<lane>
                        | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    auto single_simd_view = rng
                            | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                            | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = rng
                     | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    std::inclusive_scan(std::ranges::begin(scalar_view),
                        std::ranges::end(scalar_view),
                        std::ranges::begin(scalar_view),
                        std::plus(),
                        _mm256_cvtsi256_si32(sum));
    return 0;
}

int scan_avx2(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    auto inner_scan = [](const __m256i &v1) {
        auto s1 = _mm256_slli_si256(v1, sizeof(int));
        auto v2 = _mm256_add_epi32(v1, s1);
        auto s2 = _mm256_slli_si256(v2, 2 * sizeof(int));
        auto v3 = _mm256_add_epi32(v2, s2);
        auto p3 = _mm256_permute4x64_epi64(v3, _MM_SHUFFLE(1, 1, 1, 1));
        auto s3 = _mm256_shuffle_epi32(p3, _MM_SHUFFLE(3, 3, 3, 3));
        auto a3 = _mm256_add_epi32(v3, s3);
        return _mm256_blend_epi32(v3, a3, 0b11110000);
    };
    auto get_carry = [](const __m256i &result) {
        auto buffer = _mm256_permute4x64_epi64(result, _MM_SHUFFLE(3, 3, 3, 3));
        return _mm256_shuffle_epi32(buffer, _MM_SHUFFLE(1, 1, 1, 1));
    };
    auto sum = _mm256_setzero_si256();
    auto single_simd_view = rng
                          | simdify<lane>;
    for(auto &&simd_v : single_simd_view) {
        auto data = (__m256i*)&simd_v;
        auto block_v = _mm256_loadu_si256(data);
        auto result = inner_scan(block_v);
        auto carry = get_carry(result);
        result = _mm256_add_epi32(result, sum);
        _mm256_storeu_si256(data, result);
        sum = _mm256_add_epi32(sum, carry);
    }

    auto scalar_view = rng
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    std::inclusive_scan(std::ranges::begin(scalar_view),
                        std::ranges::end(scalar_view),
                        std::ranges::begin(scalar_view),
                        std::plus(),
                        _mm256_cvtsi256_si32(sum));
    return 0;
}

int tusenpo(std::ranges::range auto &&rng) {
    int sum = 0;
    for(auto &v : rng) {
        sum += v;
        v = sum;
    }
    return 0;
}

#if 0

template <size_t ILP = 4>
int scan_avx512(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m512i) / sizeof(int);
    constexpr auto bulk = ILP * lane;

    auto inner_scan = [](const __m512i &v0) {
        const auto zero = _mm512_setzero_si512();

        auto s1 = _mm512_alignr_epi32(v0, zero, 16 - 1);
        auto v1 = _mm512_add_epi32(v0, s1);

        auto s2 = _mm512_alignr_epi32(v1, zero, 16 - 2);
        auto v2 = _mm512_add_epi32(v1, s2);

        auto s3 = _mm512_alignr_epi32(v2, zero, 16 - 4);
        auto v3 = _mm512_add_epi32(v2, s3);

        auto s4 = _mm512_alignr_epi32(v3, zero, 16 - 8);
        auto v4 = _mm512_add_epi32(v3, s4);

        return v4;
    };

    auto get_carry = [](const __m512i &result) {
        return _mm512_permutexvar_epi32(_mm512_set1_epi32(15), result);
    };

    auto sum = _mm512_setzero_si512();

    auto process_simd = [&](auto static_for, auto simd_view) {
        constexpr auto size = static_for([]<auto>{});
        __m512i results[size] {};
        __m512i carries[size] {};
        
        for(auto &&simd_v : simd_view) {
            static_for([&, addr = &simd_v]<auto Index> {
                auto loadu = _mm512_loadu_si512((const void*)(addr + Index * lane));
                results[Index] = inner_scan(loadu);
            });

            static_for([&]<auto Index> {
                carries[Index] = get_carry(results[Index]);
            });

            static_for([&, addr = &simd_v]<auto Index> {
                auto result = _mm512_add_epi32(results[Index], sum);
                _mm512_storeu_si512((void*)(addr + Index * lane), result);
                sum = _mm512_add_epi32(sum, carries[Index]);
            });
        }
    };

    auto bulk_simd_view = rng
                        | simdify<lane>
                        | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    auto single_simd_view = rng
                          | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                          | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = rng
                     | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    
    int current_sum = _mm_cvtsi128_si32(_mm512_castsi512_si128(sum));
    
    std::inclusive_scan(std::ranges::begin(scalar_view),
                        std::ranges::end(scalar_view),
                        std::ranges::begin(scalar_view),
                        std::plus(),
                        current_sum);
    return 0;
}

#endif

// ----------------------------------------------------------------------------
// Google Benchmark
// ----------------------------------------------------------------------------

template <size_t Size>
const auto& generate_data() {
    static const auto arr = [] {
        std::array<int, Size> result;
        std::mt19937 gen{std::random_device{}()};
        // No overflow.
        std::uniform_int_distribution dist(-10, 10);
        std::ranges::generate(result, [&] { return dist(gen); });
        return result;
    } ();
    return arr;
}

template <size_t Size>
void BM_run(benchmark::State& state, auto &&func) {
    // Copy.
    auto rng = generate_data<Size>();
    for(auto _ : state) {
        auto res = func(rng);
        benchmark::DoNotOptimize(res);
        benchmark::DoNotOptimize(std::ranges::data(rng));
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size) * sizeof(int));
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

    register_test("BM_scan_avx2",
        [](auto &r) { return scan_avx2(r); });

    register_test("BM_scan_ilp<1>",
        [](auto &r) { return scan_ilp<1>(r); });

    register_test("BM_scan_ilp<2>",
        [](auto &r) { return scan_ilp<2>(r); });

    register_test("BM_scan_ilp<4>",
        [](auto &r) { return scan_ilp<4>(r); });

    register_test("BM_scan_ilp<6>",
        [](auto &r) { return scan_ilp<6>(r); });

    register_test("BM_scan_ilp<8>",
        [](auto &r) { return scan_ilp<8>(r); });

#if 0
    register_test("BM_scan_avx512<1>",
        [](auto &r) { return scan_avx512<1>(r); });

    register_test("BM_scan_avx512<2>",
        [](auto &r) { return scan_avx512<2>(r); });

    register_test("BM_scan_avx512<4>",
        [](auto &r) { return scan_avx512<4>(r); });

    register_test("BM_scan_avx512<6>",
        [](auto &r) { return scan_avx512<6>(r); });

    register_test("BM_scan_avx512<8>",
        [](auto &r) { return scan_avx512<8>(r); });
#endif

    register_test("BM_tusenpo",
        [](auto &r) { return tusenpo(r); });

    register_test("BM_std_partial_sum", [](auto &r) {
        return std::partial_sum(std::ranges::begin(r),
                                std::ranges::end(r),
                                std::ranges::begin(r));
    });

    register_test("BM_std_inclusive_scan", [](auto &r) {
        return std::inclusive_scan(std::ranges::begin(r),
                                   std::ranges::end(r),
                                   std::ranges::begin(r),
                                   std::plus(), 0);
    });

    register_test("BM_std_execution", [](auto &r) {
        return std::inclusive_scan(std::execution::unseq,
                                   std::ranges::begin(r),
                                   std::ranges::end(r),
                                   std::ranges::begin(r),
                                   std::plus(), 0);
    });

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
