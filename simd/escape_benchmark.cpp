#include <benchmark/benchmark.h>
#include <memory>
#include <string>
#include <vector>
#include <random>
#include <span>

// ═══════════════════════════════════════════════════════════════════
// 在这里 include 你的实现，或者直接把代码贴进来
// ═══════════════════════════════════════════════════════════════════

#include "escape.hpp"

// ═══════════════════════════════════════════════════════════════════
// 标量参考实现
// ═══════════════════════════════════════════════════════════════════
size_t escape_scalar(std::string_view src, std::span<char> dst) {
    size_t len = 0;
    for (char c : src) {
        if (c == '"' || c == '\\') {
            dst[len++] = '\\';
        }
        dst[len++] = c;
    }
    return len;
}

// ═══════════════════════════════════════════════════════════════════
// 测试数据生成
// ═══════════════════════════════════════════════════════════════════
struct TestData {
    std::string no_escape;      // 0% 转义
    std::string low_escape;     // 5% 转义  
    std::string mid_escape;     // 25% 转义
    std::string high_escape;    // 50% 转义
    std::string all_escape;     // 100% 转义
    
    explicit TestData(size_t size) {
        std::mt19937 rng(12345);
        
        // 0% 转义
        no_escape.resize(size);
        for (size_t i = 0; i < size; i++) {
            no_escape[i] = 'a' + (rng() % 26);
        }
        
        // 5% 转义
        low_escape.resize(size);
        for (size_t i = 0; i < size; i++) {
            if (rng() % 100 < 5) {
                low_escape[i] = (rng() % 2) ? '"' : '\\';
            } else {
                low_escape[i] = 'a' + (rng() % 26);
            }
        }
        
        // 25% 转义
        mid_escape.resize(size);
        for (size_t i = 0; i < size; i++) {
            if (rng() % 100 < 25) {
                mid_escape[i] = (rng() % 2) ? '"' : '\\';
            } else {
                mid_escape[i] = 'a' + (rng() % 26);
            }
        }
        
        // 50% 转义
        high_escape.resize(size);
        for (size_t i = 0; i < size; i++) {
            if (rng() % 2) {
                high_escape[i] = (rng() % 2) ? '"' : '\\';
            } else {
                high_escape[i] = 'a' + (rng() % 26);
            }
        }
        
        // 100% 转义
        all_escape.resize(size);
        for (size_t i = 0; i < size; i++) {
            all_escape[i] = (i % 2) ? '"' : '\\';
        }
    }
};

// 全局测试数据（避免每次 benchmark 重新生成）
static std::unique_ptr<TestData> g_data;
static std::vector<char> g_output(1 << 20);  // 1MB 输出缓冲

void ensure_test_data(size_t size) {
    static size_t current_size = 0;
    if (!g_data || current_size < size) {
        g_data = std::make_unique<TestData>(size);
        current_size = size;
    }
}

// ═══════════════════════════════════════════════════════════════════
// Benchmark 宏
// ═══════════════════════════════════════════════════════════════════

#define DEFINE_BENCHMARK(func_name, data_field, label)                      \
    static void BM_##func_name##_##label(benchmark::State& state) {         \
        size_t size = state.range(0);                                       \
        ensure_test_data(size);                                             \
        std::string_view src(g_data->data_field.data(), size);              \
        for (auto _ : state) {                                              \
            size_t len = func_name(src, std::span{g_output});               \
            benchmark::DoNotOptimize(len);                                  \
            benchmark::ClobberMemory();                                     \
        }                                                                   \
        state.SetBytesProcessed(state.iterations() * size);                 \
        state.SetLabel(#label);                                             \
    }                                                                       \
    BENCHMARK(BM_##func_name##_##label)                                     \
        ->RangeMultiplier(4)                                                \
        ->Range(64, 1 << 16)                                                \
        ->Unit(benchmark::kNanosecond)

// ═══════════════════════════════════════════════════════════════════
// 定义所有 Benchmark
// ═══════════════════════════════════════════════════════════════════

// 0% 转义 (快速路径)
DEFINE_BENCHMARK(escape_scalar,            no_escape, 0pct);
DEFINE_BENCHMARK(escape,                   no_escape, 0pct);

// 5% 转义 (典型 JSON)
DEFINE_BENCHMARK(escape_scalar,            low_escape, 5pct);
DEFINE_BENCHMARK(escape,                   low_escape, 5pct);

// 25% 转义
DEFINE_BENCHMARK(escape_scalar,            mid_escape, 25pct);
DEFINE_BENCHMARK(escape,                   mid_escape, 25pct);

// 50% 转义
DEFINE_BENCHMARK(escape_scalar,            high_escape, 50pct);
DEFINE_BENCHMARK(escape,                   high_escape, 50pct);

// 100% 转义 (最坏情况)
DEFINE_BENCHMARK(escape_scalar,            all_escape, 100pct);
DEFINE_BENCHMARK(escape,                   all_escape, 100pct);

// ═══════════════════════════════════════════════════════════════════
// 主函数
// ═══════════════════════════════════════════════════════════════════
int main(int argc, char** argv) {
    // 预热
    ensure_test_data(1 << 16);
    
    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    
    return 0;
}