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

#ifdef BENCHMARK_OPT

// ═══════════════════════════════════════════════════════════════════
// Lemire 的实现
// ═══════════════════════════════════════════════════════════════════

size_t escape_lemire_avx512(std::ranges::range auto &&src, std::ranges::range auto &&dst) {
  const auto len = stdr::size(src);
  auto in = stdr::data(src);
  auto out = stdr::data(dst);
  const char *const finalin = in + len;
  const char *const initout = out;
  __m512i solidus = _mm512_set1_epi8('\\');
  __m512i solidus16 = _mm512_set1_epi16('\\');
  __m512i quote = _mm512_set1_epi8('"');
  for (; in + 32 <= finalin; in += 32) {
    __m256i input = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(in));
    __m512i input1 = _mm512_cvtepu8_epi16(input);
    __m512i shifted_input1 = _mm512_bslli_epi128(input1, 1);

    __mmask64 is_solidus = _mm512_cmpeq_epi8_mask(input1, solidus);
    __mmask64 is_quote = _mm512_cmpeq_epi8_mask(input1, quote);
    __mmask64 is_quote_or_solidus = _kor_mask64(is_solidus, is_quote);
    __mmask64 to_keep = _kor_mask64(is_quote_or_solidus, 0xaaaaaaaaaaaaaaaa);
    __m512i escaped = _mm512_or_si512(shifted_input1, solidus16);
    _mm512_mask_compressstoreu_epi8(out, to_keep, escaped);
    out += _mm_popcnt_u64(_cvtmask64_u64(to_keep));
  }
  for (; in < finalin; in++) {
    if ((*in == '\\') || (*in == '"')) {
      *out = '\\';
      out++;
    }
    *out = *in;
    out++;
  }
  return out - initout;
}

#endif

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

#ifdef BENCHMARK_OPT
#define OPT_DEFINE_BENCHMARK(...) DEFINE_BENCHMARK(__VA_ARGS__)
#else
#define OPT_DEFINE_BENCHMARK(...)
#endif

// ═══════════════════════════════════════════════════════════════════
// 定义所有 Benchmark
// ═══════════════════════════════════════════════════════════════════

// 0% 转义 (快速路径)
DEFINE_BENCHMARK(escape_scalar,            no_escape, 0pct);
DEFINE_BENCHMARK(escape_avx2,              no_escape, 0pct);
OPT_DEFINE_BENCHMARK(escape_lemire_avx512, no_escape, 0pct);

// 5% 转义 (典型 JSON)
DEFINE_BENCHMARK(escape_scalar,            low_escape, 5pct);
DEFINE_BENCHMARK(escape_avx2,              low_escape, 5pct);
OPT_DEFINE_BENCHMARK(escape_lemire_avx512, low_escape, 5pct);

// 25% 转义
DEFINE_BENCHMARK(escape_scalar,            mid_escape, 25pct);
DEFINE_BENCHMARK(escape_avx2,              mid_escape, 25pct);
OPT_DEFINE_BENCHMARK(escape_lemire_avx512, mid_escape, 25pct);

// 50% 转义
DEFINE_BENCHMARK(escape_scalar,            high_escape, 50pct);
DEFINE_BENCHMARK(escape_avx2,              high_escape, 50pct);
OPT_DEFINE_BENCHMARK(escape_lemire_avx512, high_escape, 50pct);

// 100% 转义 (最坏情况)
DEFINE_BENCHMARK(escape_scalar,            all_escape, 100pct);
DEFINE_BENCHMARK(escape_avx2,              all_escape, 100pct);
OPT_DEFINE_BENCHMARK(escape_lemire_avx512, all_escape, 100pct);

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