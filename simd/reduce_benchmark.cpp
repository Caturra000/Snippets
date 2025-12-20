#include "reduce.hpp"
#include <benchmark/benchmark.h>
#include <array>
#include <vector>
#include <random>
#include <tuple>

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
