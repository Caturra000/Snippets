#include "lookup.hpp"
#include <benchmark/benchmark.h>
/// For test.
#include <array>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>
#include <execution>

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
