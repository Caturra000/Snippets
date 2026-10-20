// pext.cpp  —  C++23 + Google Benchmark
// PEXT (BMI2) vs AVX2 vs Scalar：Morton 解码 & 可变掩码位提取
// ----- 由 opus 编写 -----

#include <benchmark/benchmark.h>
#include <immintrin.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <numeric>
#include <print>
#include <random>
#include <span>
#include <vector>

using u64 = std::uint64_t;
using u32 = std::uint32_t;

// ---------------------------------------------------------------------------
// 编译控制：整体按 x86-64 基线编译，SIMD 函数用 target 属性单独开启指令集，
// 这样标量版本不会被偷偷编译成 BMI2/AVX2 代码，二进制也能运行时检测。
// ---------------------------------------------------------------------------
#define TARGET_BMI2 [[gnu::target("bmi2")]]
#define TARGET_AVX2 [[gnu::target("avx2")]]

#if defined(__clang__)
#  define NOVEC_LOOP _Pragma("clang loop vectorize(disable) interleave(disable)")
#elif defined(__GNUC__) && __GNUC__ >= 14
#  define NOVEC_LOOP _Pragma("GCC novector")
#else
#  define NOVEC_LOOP
#endif

constexpr u64 kEven = 0x5555'5555'5555'5555ULL;
constexpr u64 kOdd  = 0xAAAA'AAAA'AAAA'AAAAULL;

// ---------------------------------------------------------------------------
// 标量参考实现
// ---------------------------------------------------------------------------
constexpr u64 pext_soft(u64 v, u64 m) noexcept {
    u64 r = 0;
    for (u64 bit = 1; m != 0; bit <<= 1) {
        r |= (v & m & (~m + 1)) ? bit : 0;  // m & -m = 最低位的 1
        m &= m - 1;
    }
    return r;
}
static_assert(pext_soft(0b1011'0110, 0b1111'0000) == 0b1011);
static_assert(pext_soft(0b0101, kEven) == 0b11);

constexpr u32 compact_even(u64 x) noexcept {
    x &= kEven;
    x = (x | (x >> 1))  & 0x3333'3333'3333'3333ULL;
    x = (x | (x >> 2))  & 0x0F0F'0F0F'0F0F'0F0FULL;
    x = (x | (x >> 4))  & 0x00FF'00FF'00FF'00FFULL;
    x = (x | (x >> 8))  & 0x0000'FFFF'0000'FFFFULL;
    x = (x | (x >> 16)) & 0x0000'0000'FFFF'FFFFULL;
    return static_cast<u32>(x);
}
static_assert(compact_even(0b0101) == 0b11);
static_assert(compact_even(~0ULL) == 0xFFFF'FFFFu);

using DecodeFn = void (*)(std::span<const u64>, std::span<u32>, std::span<u32>);

static void decode_scalar_loop(std::span<const u64> in, std::span<u32> xs, std::span<u32> ys) {
    NOVEC_LOOP
    for (std::size_t i = 0; i < in.size(); ++i) {
        xs[i] = static_cast<u32>(pext_soft(in[i], kEven));
        ys[i] = static_cast<u32>(pext_soft(in[i], kOdd));
    }
}

static void decode_scalar_magic(std::span<const u64> in, std::span<u32> xs, std::span<u32> ys) {
    NOVEC_LOOP
    for (std::size_t i = 0; i < in.size(); ++i) {
        xs[i] = compact_even(in[i]);
        ys[i] = compact_even(in[i] >> 1);
    }
}

// 同样的标量代码，但允许编译器使用 AVX2 自动向量化
TARGET_AVX2 static void decode_scalar_autovec(std::span<const u64> in, std::span<u32> xs, std::span<u32> ys) {
    for (std::size_t i = 0; i < in.size(); ++i) {
        xs[i] = compact_even(in[i]);
        ys[i] = compact_even(in[i] >> 1);
    }
}

// ---------------------------------------------------------------------------
// BMI2 PEXT
// ---------------------------------------------------------------------------
TARGET_BMI2 static void decode_pext(std::span<const u64> in, std::span<u32> xs, std::span<u32> ys) {
    for (std::size_t i = 0; i < in.size(); ++i) {
        xs[i] = static_cast<u32>(_pext_u64(in[i], kEven));
        ys[i] = static_cast<u32>(_pext_u64(in[i], kOdd));
    }
}

// ---------------------------------------------------------------------------
// AVX2：4 个 u64 并行做 magic-bits 压缩，压到字节粒度后用 pshufb + permd 收尾
// 返回 [x0 x1 x2 x3 | y0 y1 y2 y3]（8 x u32）
// ---------------------------------------------------------------------------
TARGET_AVX2 [[gnu::always_inline]] static inline __m256i morton_compact4(__m256i v) {
    const __m256i m0 = _mm256_set1_epi64x(0x5555'5555'5555'5555LL);
    const __m256i m1 = _mm256_set1_epi64x(0x3333'3333'3333'3333LL);
    const __m256i m2 = _mm256_set1_epi64x(0x0F0F'0F0F'0F0F'0F0FLL);
    const __m256i m3 = _mm256_set1_epi64x(0x00FF'00FF'00FF'00FFLL);

    __m256i x = _mm256_and_si256(v, m0);
    __m256i y = _mm256_and_si256(_mm256_srli_epi64(v, 1), m0);

    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_srli_epi64(x, 1)), m1);
    y = _mm256_and_si256(_mm256_or_si256(y, _mm256_srli_epi64(y, 1)), m1);
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_srli_epi64(x, 2)), m2);
    y = _mm256_and_si256(_mm256_or_si256(y, _mm256_srli_epi64(y, 2)), m2);
    x = _mm256_and_si256(_mm256_or_si256(x, _mm256_srli_epi64(x, 4)), m3);
    y = _mm256_and_si256(_mm256_or_si256(y, _mm256_srli_epi64(y, 4)), m3);

    // 此时每个 qword 里 x 的 4 个有效字节在偶数字节位，y 左移 8 位放进奇数字节位
    __m256i z = _mm256_or_si256(x, _mm256_slli_epi64(y, 8));

    // 每个 128-bit lane：[x(c0) y(c0) x(c1) y(c1)]
    const __m256i shuf = _mm256_setr_epi8(
        0, 2, 4, 6, 1, 3, 5, 7, 8, 10, 12, 14, 9, 11, 13, 15,
        0, 2, 4, 6, 1, 3, 5, 7, 8, 10, 12, 14, 9, 11, 13, 15);
    z = _mm256_shuffle_epi8(z, shuf);

    // [x0 y0 x1 y1 x2 y2 x3 y3] -> [x0 x1 x2 x3 y0 y1 y2 y3]
    const __m256i perm = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);
    return _mm256_permutevar8x32_epi32(z, perm);
}

TARGET_AVX2 static void decode_avx2(std::span<const u64> in, std::span<u32> xs, std::span<u32> ys) {
    const std::size_t n = in.size();
    std::size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        const __m256i a  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in.data() + i));
        const __m256i b  = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in.data() + i + 4));
        const __m256i za = morton_compact4(a);  // [x0..x3 | y0..y3]
        const __m256i zb = morton_compact4(b);  // [x4..x7 | y4..y7]
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(xs.data() + i), _mm256_permute2x128_si256(za, zb, 0x20));
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(ys.data() + i), _mm256_permute2x128_si256(za, zb, 0x31));
    }
    for (; i < n; ++i) {  // 尾部
        xs[i] = compact_even(in[i]);
        ys[i] = compact_even(in[i] >> 1);
    }
}

// ---------------------------------------------------------------------------
// 可变掩码：每个元素掩码不同（SIMD 无直接等价物）
// ---------------------------------------------------------------------------
static void varmask_soft(std::span<const u64> v, std::span<const u64> m, std::span<u64> out) {
    NOVEC_LOOP
    for (std::size_t i = 0; i < v.size(); ++i) out[i] = pext_soft(v[i], m[i]);
}

TARGET_BMI2 static void varmask_pext(std::span<const u64> v, std::span<const u64> m, std::span<u64> out) {
    for (std::size_t i = 0; i < v.size(); ++i) out[i] = _pext_u64(v[i], m[i]);
}

// ---------------------------------------------------------------------------
// 数据生成
// ---------------------------------------------------------------------------
static std::vector<u64> make_random(std::size_t n, u64 seed) {
    std::mt19937_64 rng(seed);
    std::vector<u64> v(n);
    std::ranges::generate(v, std::ref(rng));
    return v;
}

static std::vector<u64> make_masks(std::size_t n, int popcnt, u64 seed) {
    std::mt19937_64 rng(seed);
    std::vector<u64> v(n);
    std::array<int, 64> pos{};
    for (auto& m : v) {
        std::iota(pos.begin(), pos.end(), 0);
        m = 0;
        for (int j = 0; j < popcnt; ++j) {  // 部分 Fisher-Yates
            std::swap(pos[j], pos[j + rng() % (64 - j)]);
            m |= 1ULL << pos[j];
        }
    }
    return v;
}

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------
template <DecodeFn Fn>
static void BM_Morton(benchmark::State& st) {
    const auto n = static_cast<std::size_t>(st.range(0));
    const auto in = make_random(n, 42);
    std::vector<u32> xs(n), ys(n);

    for (auto _ : st) {
        Fn(in, xs, ys);
        benchmark::DoNotOptimize(xs.data());
        benchmark::DoNotOptimize(ys.data());
        benchmark::ClobberMemory();
    }
    const auto items = static_cast<int64_t>(st.iterations()) * static_cast<int64_t>(n);
    st.SetItemsProcessed(items);
    st.SetBytesProcessed(items * static_cast<int64_t>(sizeof(u64) + 2 * sizeof(u32)));
}

using VarFn = void (*)(std::span<const u64>, std::span<const u64>, std::span<u64>);

template <VarFn Fn>
static void BM_VarMask(benchmark::State& st) {
    const auto n      = static_cast<std::size_t>(st.range(0));
    const int  popcnt = static_cast<int>(st.range(1));
    const auto v = make_random(n, 1);
    const auto m = make_masks(n, popcnt, 2);
    std::vector<u64> out(n);

    for (auto _ : st) {
        Fn(v, m, out);
        benchmark::DoNotOptimize(out.data());
        benchmark::ClobberMemory();
    }
    st.SetItemsProcessed(static_cast<int64_t>(st.iterations()) * static_cast<int64_t>(n));
}

// ---------------------------------------------------------------------------
// 正确性校验
// ---------------------------------------------------------------------------
static bool verify(bool bmi2, bool avx2) {
    constexpr std::size_t n = 4099;  // 非 8 的倍数，覆盖尾部处理
    const auto in = make_random(n, 7);
    std::vector<u32> rx(n), ry(n), xs(n), ys(n);
    decode_scalar_loop(in, rx, ry);

    bool all_ok = true;
    auto check = [&](const char* name, DecodeFn fn) {
        std::ranges::fill(xs, 0u);
        std::ranges::fill(ys, 0u);
        fn(in, xs, ys);
        const bool ok = (xs == rx) && (ys == ry);
        std::println("verify {:<16} {}", name, ok ? "OK" : "FAILED");
        all_ok &= ok;
    };
    check("Scalar_Magic", decode_scalar_magic);
    if (avx2) check("Scalar_AutoVec", decode_scalar_autovec);
    if (bmi2) check("PEXT", decode_pext);
    if (avx2) check("AVX2", decode_avx2);

    if (bmi2) {
        const auto m = make_masks(n, 23, 9);
        std::vector<u64> a(n), b(n);
        varmask_soft(in, m, a);
        varmask_pext(in, m, b);
        const bool ok = (a == b);
        std::println("verify {:<16} {}", "VarMask_PEXT", ok ? "OK" : "FAILED");
        all_ok &= ok;
    }
    return all_ok;
}

int main(int argc, char** argv) {
    __builtin_cpu_init();
    const bool has_bmi2 = __builtin_cpu_supports("bmi2");
    const bool has_avx2 = __builtin_cpu_supports("avx2");
    std::println("CPU features: BMI2={} AVX2={}", has_bmi2, has_avx2);

    if (!verify(has_bmi2, has_avx2)) {
        std::println(stderr, "verification failed, abort.");
        return 1;
    }

    auto morton = [](const char* name, auto fn) {
        benchmark::RegisterBenchmark(name, fn)
            ->RangeMultiplier(8)
            ->Range(1 << 10, 1 << 22)  // 1K..4M 个元素：L1 -> L2 -> L3 -> DRAM
            ->Unit(benchmark::kMicrosecond);
    };
    morton("Morton/Scalar_Loop",  &BM_Morton<decode_scalar_loop>);
    morton("Morton/Scalar_Magic", &BM_Morton<decode_scalar_magic>);
    if (has_avx2) morton("Morton/Scalar_AutoVec", &BM_Morton<decode_scalar_autovec>);
    if (has_bmi2) morton("Morton/PEXT",           &BM_Morton<decode_pext>);
    if (has_avx2) morton("Morton/AVX2",           &BM_Morton<decode_avx2>);

    auto var = [](const char* name, auto fn) {
        benchmark::RegisterBenchmark(name, fn)
            ->ArgsProduct({{1 << 14}, {4, 16, 32, 48, 60}})  // {n, mask popcount}
            ->ArgNames({"n", "popcnt"})
            ->Unit(benchmark::kMicrosecond);
    };
    var("VarMask/Scalar", &BM_VarMask<varmask_soft>);
    if (has_bmi2) var("VarMask/PEXT", &BM_VarMask<varmask_pext>);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
}