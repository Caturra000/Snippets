#include <benchmark/benchmark.h>
#include <immintrin.h>
#include <array>
#include <random>
#include <algorithm>
#include <vector>
#include <iostream>

// 大部分工作是LLM完成的，用于反汇编测试一个非常小的局部
//
// 功能：
// 接受一个array<char,32>，然后要返回2个m256i
// 第一个低半部（m128i）只拿源的奇数下标，高版本复制低版本内容
// 第二个类似，但是只拿偶数

// 背景：
// 其实是某个库的实现是暴力拿union YMM,XMM,U64[]做的
// 然后看编译器的自动向量化非常绷不住（clang-20直接sequence insert，了不起）
// 进一步测试一下不同版本
//
// 顺便提供生成观测用的源程序
/*
#include <immintrin.h>
#include <stdint.h>
#include <string.h> // for memcpy if needed

union M256_Union {
    __m256i ymm;
    __m128i xmm[2];
    uint8_t u8[32];
};

extern void do_stuff(__m256i&,__m256i&);

void split_union_scalar(const char* input) {
    M256_Union u_odd, u_even;

    // 标量方式填充：CPU 需要一个字节一个字节地搬运
    for (int i = 0; i < 16; ++i) {
        uint8_t e = input[2 * i];     // 取偶数下标
        uint8_t o = input[2 * i + 1]; // 取奇数下标

        // 填充低 128 位 (前16字节)
        u_even.u8[i] = e;
        u_odd.u8[i]  = o;

        // 填充高 128 位 (后16字节，也就是复制)
        u_even.u8[i + 16] = e;
        u_odd.u8[i + 16]  = o;
    }

    do_stuff(u_odd.ymm, u_even.ymm);
}
*/








// ----------------------------------------------------------------------------
// Mock Function
// ----------------------------------------------------------------------------
__attribute__((noinline)) void do_stuff(__m256i& evens, __m256i& odds) {
    benchmark::DoNotOptimize(evens);
    benchmark::DoNotOptimize(odds);
}

// ----------------------------------------------------------------------------
// Implementations
// ----------------------------------------------------------------------------

void split_insert_clang20(char const* ptr) {
    __m128i xmm0 = _mm_cvtsi32_si128((uint8_t)ptr[0]);
    xmm0 = _mm_insert_epi8(xmm0, ptr[2], 1);
    xmm0 = _mm_insert_epi8(xmm0, ptr[4], 2);
    xmm0 = _mm_insert_epi8(xmm0, ptr[6], 3);
    xmm0 = _mm_insert_epi8(xmm0, ptr[8], 4);
    xmm0 = _mm_insert_epi8(xmm0, ptr[10], 5);
    xmm0 = _mm_insert_epi8(xmm0, ptr[12], 6);
    xmm0 = _mm_insert_epi8(xmm0, ptr[14], 7);
    xmm0 = _mm_insert_epi8(xmm0, ptr[16], 8);
    xmm0 = _mm_insert_epi8(xmm0, ptr[18], 9);
    xmm0 = _mm_insert_epi8(xmm0, ptr[20], 10);
    xmm0 = _mm_insert_epi8(xmm0, ptr[22], 11);
    xmm0 = _mm_insert_epi8(xmm0, ptr[24], 12);
    xmm0 = _mm_insert_epi8(xmm0, ptr[26], 13);
    xmm0 = _mm_insert_epi8(xmm0, ptr[28], 14);
    xmm0 = _mm_insert_epi8(xmm0, ptr[30], 15);

    __m128i xmm1 = _mm_cvtsi32_si128((uint8_t)ptr[1]);
    xmm1 = _mm_insert_epi8(xmm1, ptr[3], 1);
    xmm1 = _mm_insert_epi8(xmm1, ptr[5], 2);
    xmm1 = _mm_insert_epi8(xmm1, ptr[7], 3);
    xmm1 = _mm_insert_epi8(xmm1, ptr[9], 4);
    xmm1 = _mm_insert_epi8(xmm1, ptr[11], 5);
    xmm1 = _mm_insert_epi8(xmm1, ptr[13], 6);
    xmm1 = _mm_insert_epi8(xmm1, ptr[15], 7);
    xmm1 = _mm_insert_epi8(xmm1, ptr[17], 8);
    xmm1 = _mm_insert_epi8(xmm1, ptr[19], 9);
    xmm1 = _mm_insert_epi8(xmm1, ptr[21], 10);
    xmm1 = _mm_insert_epi8(xmm1, ptr[23], 11);
    xmm1 = _mm_insert_epi8(xmm1, ptr[25], 12);
    xmm1 = _mm_insert_epi8(xmm1, ptr[27], 13);
    xmm1 = _mm_insert_epi8(xmm1, ptr[29], 14);
    xmm1 = _mm_insert_epi8(xmm1, ptr[31], 15);

    __m256i ymm0 = _mm256_castsi128_si256(xmm0);
    __m256i ymm1 = _mm256_castsi128_si256(xmm1);

    ymm0 = _mm256_permute4x64_epi64(ymm0, 0x44);
    ymm1 = _mm256_permute4x64_epi64(ymm1, 0x44);

    do_stuff(ymm0, ymm1);
}

// GCC 15?
// sonnet不知道哪里抄来的，但是还是拿来测了
void split_shuffle_gcc15(char const* ptr) {
    __m128i src_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
    __m128i src_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 16));

    __m128i mask_evens = _mm_setr_epi8(
        0, 2, 4, 6, 8, 10, 12, 14, -1, -1, -1, -1, -1, -1, -1, -1
    );
    __m128i mask_odds = _mm_setr_epi8(
        1, 3, 5, 7, 9, 11, 13, 15, -1, -1, -1, -1, -1, -1, -1, -1
    );

    __m128i e1 = _mm_shuffle_epi8(src_lo, mask_evens);
    __m128i e2 = _mm_shuffle_epi8(src_hi, mask_evens);
    __m128i xmm_evens = _mm_unpacklo_epi64(e1, e2);

    __m128i o1 = _mm_shuffle_epi8(src_lo, mask_odds);
    __m128i o2 = _mm_shuffle_epi8(src_hi, mask_odds);
    __m128i xmm_odds = _mm_unpacklo_epi64(o1, o2);

    __m256i ymm_evens = _mm256_broadcastsi128_si256(xmm_evens);
    __m256i ymm_odds  = _mm256_broadcastsi128_si256(xmm_odds);

    do_stuff(ymm_evens, ymm_odds);
}

void split_bitwise_gcc15_old(const char* input) {
    __m128i xmm1 = _mm_setzero_si128();
    xmm1 = _mm_cmpeq_epi32(xmm1, xmm1);

    alignas(32) __m256i stack_var0;
    alignas(32) __m256i stack_var1;
    
    auto* arg0_ptr = reinterpret_cast<__m128i*>(&stack_var0);
    auto* arg1_ptr = reinterpret_cast<__m128i*>(&stack_var1);

    xmm1 = _mm_srli_epi16(xmm1, 8);

    __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
    __m128i xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + 16));

    __m128i xmm2 = _mm_and_si128(xmm0, xmm1);

    xmm1 = _mm_and_si128(xmm3, xmm1);

    xmm0 = _mm_srli_epi16(xmm0, 8);
    xmm3 = _mm_srli_epi16(xmm3, 8);

    xmm1 = _mm_packus_epi16(xmm2, xmm1);

    xmm0 = _mm_packus_epi16(xmm0, xmm3);

    _mm_store_si128(arg1_ptr, xmm1);

    _mm_store_si128(arg0_ptr, xmm0);

    _mm_store_si128(arg1_ptr + 1, xmm1);

    _mm_store_si128(arg0_ptr + 1, xmm0);

    do_stuff(stack_var0, stack_var1);
}

void split_bitwise_gcc14_old(const char* input) {
    __m128i mask = _mm_set1_epi32(0x00FF00FF);

    __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
    __m128i xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + 16));

    __m128i xmm2 = _mm_and_si128(xmm0, mask);
    __m128i xmm1 = _mm_and_si128(xmm3, mask);

    xmm0 = _mm_srli_epi16(xmm0, 8);
    xmm3 = _mm_srli_epi16(xmm3, 8);

    xmm1 = _mm_packus_epi16(xmm2, xmm1);

    xmm0 = _mm_packus_epi16(xmm0, xmm3);

    alignas(32) __m256i arg0;
    alignas(32) __m256i arg1;

    auto* arg0_ptr = reinterpret_cast<__m128i*>(&arg0);
    auto* arg1_ptr = reinterpret_cast<__m128i*>(&arg1);

    _mm_store_si128(arg1_ptr, xmm1);

    _mm_store_si128(arg0_ptr, xmm0);

    _mm_store_si128(arg1_ptr + 1, xmm1);

    _mm_store_si128(arg0_ptr + 1, xmm0);

    do_stuff(arg0, arg1);
}


void split_bitwise_gcc15(char const* input) {
    __m128i mask = _mm_setzero_si128();
    mask = _mm_cmpeq_epi32(mask, mask);
    mask = _mm_srli_epi16(mask, 8);

    __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
    __m128i xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + 16));

    __m128i xmm2 = _mm_and_si128(xmm0, mask);
    __m128i xmm1 = _mm_and_si128(xmm3, mask);

    xmm0 = _mm_srli_epi16(xmm0, 8);
    xmm3 = _mm_srli_epi16(xmm3, 8);

    xmm1 = _mm_packus_epi16(xmm2, xmm1);
    xmm0 = _mm_packus_epi16(xmm0, xmm3);

    __m256i arg0 = _mm256_set_m128i(xmm0, xmm0);
    __m256i arg1 = _mm256_set_m128i(xmm1, xmm1);

    do_stuff(arg0, arg1);
}

void split_bitwise_gcc14(char const* input) {
    __m128i mask = _mm_set1_epi32(0x00FF00FF);

    __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input));
    __m128i xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(input + 16));

    __m128i xmm2 = _mm_and_si128(xmm0, mask);
    __m128i xmm1 = _mm_and_si128(xmm3, mask);

    xmm0 = _mm_srli_epi16(xmm0, 8);
    xmm3 = _mm_srli_epi16(xmm3, 8);

    xmm1 = _mm_packus_epi16(xmm2, xmm1);
    xmm0 = _mm_packus_epi16(xmm0, xmm3);

    __m256i arg0 = _mm256_set_m128i(xmm0, xmm0);
    __m256i arg1 = _mm256_set_m128i(xmm1, xmm1);

    do_stuff(arg0, arg1);
}

// ----------------------------------------------------------------------------
// Benchmark Framework
// ----------------------------------------------------------------------------

template <size_t Size>
const auto& generate_data() {
    static const auto arr = [] {
        std::array<char, Size> result;
        std::mt19937 gen{std::random_device{}()};
        std::uniform_int_distribution<int> dist(-128, 127);
        std::ranges::generate(result, [&] { return static_cast<char>(dist(gen)); });
        return result;
    } ();
    return arr;
}

template <size_t Size>
void BM_run(benchmark::State& state, auto &&func) {
    const auto& rng = generate_data<Size>();

    for(auto _ : state) {
        func(rng);
        benchmark::ClobberMemory();
    }

    state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(Size));
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

    auto register_test_impl = [seq](auto name, auto func) {
        register_tests(seq, name, func);
    };

    auto wrapper = [](auto &&f) {
        return [f](const auto& r) {
            const char* ptr = r.data();
            size_t size = r.size();
            for (size_t i = 0; i + 32 <= size; i += 32) {
                f(ptr + i);
            }
        };
    };

    // Obviously, we can use lambda as a wrapper for modern C++,
    // but it seems that performance regression is really bad. (both GCC and Clang.)
    // Use macro instead as a workaround.
    // TODO: check assembly.
    #define register_test(name, f) \
        register_test_impl(name, \
        [](const auto& r) { \
            const char* ptr = r.data();\
            size_t size = r.size();\
            for (size_t i = 0; i + 32 <= size; i += 32) {\
                f(ptr + i);\
            }\
        })

    register_test("BM_split_insert_clang20", split_insert_clang20);
    register_test("BM_split_shuffle_gcc15", split_shuffle_gcc15);
    register_test("BM_split_bitwise_gcc15_old", split_bitwise_gcc15_old);
    register_test("BM_split_bitwise_gcc14_old", split_bitwise_gcc14_old);
    register_test("BM_split_bitwise_gcc15", split_bitwise_gcc15);
    register_test("BM_split_bitwise_gcc14", split_bitwise_gcc14);

    #undef register_test

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}


// Clang 20
/*
split_union_scalar(char const*):
        pushq   %rbp
        movq    %rsp, %rbp
        andq    $-32, %rsp
        subq    $96, %rsp
        movzbl  (%rdi), %eax
        movq    %rsp, %rsi
        vmovd   %eax, %xmm0
        movzbl  1(%rdi), %eax
        vpinsrb $1, 2(%rdi), %xmm0, %xmm0
        vpinsrb $2, 4(%rdi), %xmm0, %xmm0
        vmovd   %eax, %xmm1
        vpinsrb $1, 3(%rdi), %xmm1, %xmm1
        vpinsrb $3, 6(%rdi), %xmm0, %xmm0
        vpinsrb $2, 5(%rdi), %xmm1, %xmm1
        vpinsrb $4, 8(%rdi), %xmm0, %xmm0
        vpinsrb $3, 7(%rdi), %xmm1, %xmm1
        vpinsrb $5, 10(%rdi), %xmm0, %xmm0
        vpinsrb $4, 9(%rdi), %xmm1, %xmm1
        vpinsrb $6, 12(%rdi), %xmm0, %xmm0
        vpinsrb $5, 11(%rdi), %xmm1, %xmm1
        vpinsrb $7, 14(%rdi), %xmm0, %xmm0
        vpinsrb $6, 13(%rdi), %xmm1, %xmm1
        vpinsrb $8, 16(%rdi), %xmm0, %xmm0
        vpinsrb $7, 15(%rdi), %xmm1, %xmm1
        vpinsrb $9, 18(%rdi), %xmm0, %xmm0
        vpinsrb $8, 17(%rdi), %xmm1, %xmm1
        vpinsrb $10, 20(%rdi), %xmm0, %xmm0
        vpinsrb $9, 19(%rdi), %xmm1, %xmm1
        vpinsrb $11, 22(%rdi), %xmm0, %xmm0
        vpinsrb $10, 21(%rdi), %xmm1, %xmm1
        vpinsrb $12, 24(%rdi), %xmm0, %xmm0
        vpinsrb $11, 23(%rdi), %xmm1, %xmm1
        vpinsrb $13, 26(%rdi), %xmm0, %xmm0
        vpinsrb $12, 25(%rdi), %xmm1, %xmm1
        vpinsrb $14, 28(%rdi), %xmm0, %xmm0
        vpinsrb $13, 27(%rdi), %xmm1, %xmm1
        vpinsrb $15, 30(%rdi), %xmm0, %xmm0
        vpinsrb $14, 29(%rdi), %xmm1, %xmm1
        vpinsrb $15, 31(%rdi), %xmm1, %xmm1
        leaq    32(%rsp), %rdi
        vpermq  $68, %ymm0, %ymm0
        vpermq  $68, %ymm1, %ymm1
        vmovdqa %ymm0, (%rsp)
        vmovdqa %ymm1, 32(%rsp)
        vzeroupper
        callq   do_stuff(long long vector[4]&, long long vector[4]&)@PLT
        movq    %rbp, %rsp
        popq    %rbp
        retq
*/

// GCC14
/*
split_union_scalar(char const*):
        pushq   %rbp
        movl    $16711935, %eax
        movq    %rsp, %rbp
        andq    $-32, %rsp
        vmovd   %eax, %xmm1
        subq    $64, %rsp
        vmovdqu (%rdi), %xmm0
        vmovdqu 16(%rdi), %xmm3
        vpbroadcastd    %xmm1, %xmm1
        leaq    32(%rsp), %rsi
        movq    %rsp, %rdi
        vpand   %xmm0, %xmm1, %xmm2
        vpand   %xmm3, %xmm1, %xmm1
        vpsrlw  $8, %xmm0, %xmm0
        vpsrlw  $8, %xmm3, %xmm3
        vpackuswb       %xmm1, %xmm2, %xmm1
        vpackuswb       %xmm3, %xmm0, %xmm0
        vmovdqa %xmm1, 32(%rsp)
        vmovdqa %xmm0, (%rsp)
        vmovdqa %xmm1, 48(%rsp)
        vmovdqa %xmm0, 16(%rsp)
        call    do_stuff(long long vector[4]&, long long vector[4]&)
        leave
        ret
*/

// GCC15
/*
split_union_scalar(char const*):
        pushq   %rbp
        vpcmpeqd        %xmm1, %xmm1, %xmm1
        movq    %rsp, %rbp
        andq    $-32, %rsp
        vpsrlw  $8, %xmm1, %xmm1
        subq    $64, %rsp
        vmovdqu (%rdi), %xmm0
        vmovdqu 16(%rdi), %xmm3
        leaq    32(%rsp), %rsi
        movq    %rsp, %rdi
        vpand   %xmm0, %xmm1, %xmm2
        vpand   %xmm3, %xmm1, %xmm1
        vpsrlw  $8, %xmm0, %xmm0
        vpsrlw  $8, %xmm3, %xmm3
        vpackuswb       %xmm1, %xmm2, %xmm1
        vpackuswb       %xmm3, %xmm0, %xmm0
        vmovdqa %xmm1, 32(%rsp)
        vmovdqa %xmm0, (%rsp)
        vmovdqa %xmm1, 48(%rsp)
        vmovdqa %xmm0, 16(%rsp)
        call    do_stuff(long long vector[4]&, long long vector[4]&)
        leave
        ret
*/
