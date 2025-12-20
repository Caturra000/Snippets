#include "scan.hpp"
#include <benchmark/benchmark.h>
#include <array>
#include <random>
#include <tuple>
#include <numeric>
#include <execution>

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
