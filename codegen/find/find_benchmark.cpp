#include <benchmark/benchmark.h>
#include <vector>
#include <cstdlib>

auto make(auto &bm, auto size) {
    auto size_with_sentinel = size + 1;
    std::vector<int> vec(size_with_sentinel);
    for(auto i = 0; auto &v : vec) v = ++i;
    benchmark::DoNotOptimize(vec);
    return vec;
}

bool find(int* arr, size_t size, int val) {
    #pragma clang loop unroll_count(4)
    for(size_t i = 0; i < size; ++i) {
        if(val == arr[i]) return true;
    }
    return false;
}

bool find_v2(int* arr, size_t size, int val) {
    arr[size] = val;

    #pragma clang loop unroll_count(4)
    for(size_t i = 0; ; ++i) {
        if(val == arr[i]) return i != size;
    }
    return false;
}

template <auto F>
void bench_template(benchmark::State& state, int target_index) {
    auto size = state.range(0);
    auto vec = make(state, size);
    for(auto _ : state) {
        auto index = target_index;
        auto old = vec[size];
        benchmark::DoNotOptimize(index);
        auto target = vec[index];
        benchmark::DoNotOptimize(target);
        auto result = F(vec.data(), size, target);
        benchmark::DoNotOptimize(result);
        vec[size] = old;
    }
}

void BM_find_best_case(benchmark::State& state) {
    bench_template<find>(state, 0);
}

void BM_find_v2_best_case(benchmark::State& state) {
    bench_template<find_v2>(state, 0);
}

void BM_find_worst_case(benchmark::State& state) {
    bench_template<find>(state, state.range(0));
}

void BM_find_v2_worst_case(benchmark::State& state) {
    bench_template<find_v2>(state, state.range(0));
}

void BM_find_average_case(benchmark::State& state) {
    bench_template<find>(state, state.range(0) / 2);
}

void BM_find_v2_average_case(benchmark::State& state) {
    bench_template<find_v2>(state, state.range(0) / 2);
}

#define FIND_BM_(name) BENCHMARK(name)->RangeMultiplier(2)->Range(8, 1<<26)
FIND_BM_(BM_find_best_case);
FIND_BM_(BM_find_v2_best_case);
FIND_BM_(BM_find_worst_case);
FIND_BM_(BM_find_v2_worst_case);
FIND_BM_(BM_find_average_case);
FIND_BM_(BM_find_v2_average_case);
#undef FIND_BM_

BENCHMARK_MAIN();
