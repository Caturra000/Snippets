#include <benchmark/benchmark.h>
#include <x86intrin.h>
#include <algorithm>
#include <ranges>
#include <array>
#include <vector>
#include <random>
#include <tuple>

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

int sum_avx2(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    __m256i partial_sum {};

    auto simd_view = rng
                   | simdify<lane>;
    for(auto &&simd_v : simd_view) {
        auto addr = &simd_v;
        auto loadu = _mm256_loadu_si256((__m256i*)addr);
        partial_sum = _mm256_add_epi32(partial_sum, loadu);
    }

    auto scalar_view = rng
                     | std::views::drop(lane * std::ranges::size(simd_view));
    int sum = std::ranges::fold_left(scalar_view, 0, std::plus());

    int temp[lane];
    _mm256_storeu_si256((__m256i*)std::ranges::data(temp), partial_sum);
    sum = std::ranges::fold_left(temp, sum, std::plus());

    return sum;
}

template <size_t ILP = 4>
int sum_avx2_ilp(std::ranges::range auto &&rng) {
    constexpr auto lane = sizeof(__m256i) / sizeof(int);
    constexpr auto bulk = lane * ILP;
    __m256i partial_sum[ILP] {};
    auto process_simd = [&](auto konstexpr_for, auto simd_view) {
        for(auto &&simd_v : simd_view) {
            konstexpr_for([&, addr = &simd_v]<auto Index> {
                auto &partial = partial_sum[Index];
                auto loadu = _mm256_loadu_si256(
                    (__m256i*)(addr + Index * lane));
                partial = _mm256_add_epi32(partial, loadu);
            });
        }
    };

    // SIMD+ILP
    auto bulk_simd_view = rng
                        | simdify<lane>
                        | simdify<ILP>;
    process_simd(constexpr_for<0, ILP>, bulk_simd_view);

    // SIMD
    auto single_simd_view = rng
                          | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                          | simdify<lane>;
    process_simd(constexpr_for<0, 1>, single_simd_view);

    auto scalar_view = rng
                     | std::views::drop(bulk * std::ranges::size(bulk_simd_view))
                     | std::views::drop(lane * std::ranges::size(single_simd_view));
    int sum = std::ranges::fold_left(scalar_view, 0, std::plus());

    constexpr_for<1, ILP>([&]<size_t Index> {
        partial_sum[0] = _mm256_add_epi32(partial_sum[0], partial_sum[Index]);
    });
    int temp[lane];
    _mm256_storeu_si256((__m256i*)std::ranges::data(temp), partial_sum[0]);
    sum = std::ranges::fold_left(temp, sum, std::plus());

    return sum;
}

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
    const auto &rng = generate_data<Size>();

    for(auto _ : state) {
        auto res = func(rng);
        benchmark::DoNotOptimize(res);
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

    register_test("BM_sum_avx2",
        [](const auto &r) { return sum_avx2(r); });

    register_test("BM_sum_avx2_ilp<4>",
        [](const auto &r) { return sum_avx2_ilp<4>(r); });

    register_test("BM_sum_std_fold_left",
        [](const auto &r) { return std::ranges::fold_left(r, 0, std::plus()); });

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
