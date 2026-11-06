// 参考：https://stackoverflow.com/questions/46135369/what-are-the-costs-of-failed-store-to-load-forwarding-on-x86
//
// i9-13900hx, clang++-22 -O3 -std=c++20:
// init fused copy fused     0.236728s
// init fused copy unfused   0.444923s
// init unfused copy fused   4.07242s
// init unfused copy unfused 0.427176s

#include <chrono>
#include <iostream>

struct alignas(16) S
{
    char* a;
    int* b;
};

extern "C" void init_fused_copy_unfused(int n, S & s2, S & s1);
extern "C" void init_fused_copy_fused(int n, S & s2, S & s1);
extern "C" void init_unfused_copy_unfused(int n, S & s2, S & s1);
extern "C" void init_unfused_copy_fused(int n, S & s2, S & s1);

asm(R"(
.data
.align 16
c:  .byte 0
.align 4
i:  .long 0
.align 16
s:  .quad c, i

.text

.globl init_fused_copy_fused
.align 16
init_fused_copy_fused:
    movups  s(%rip), %xmm0
    movups  %xmm0, (%rdx)

    movups  (%rdx), %xmm1
    movups  %xmm1, (%rsi)

    decl    %edi
    jnz     init_fused_copy_fused
    ret

.globl init_unfused_copy_fused
.align 16
init_unfused_copy_fused:
    leaq    c(%rip), %rax
    movq    %rax, (%rdx)
    leaq    i(%rip), %rax
    movq    %rax, 8(%rdx)

    movups  (%rdx), %xmm1
    movups  %xmm1, (%rsi)

    decl    %edi
    jnz     init_unfused_copy_fused
    ret

.globl init_fused_copy_unfused
.align 16
init_fused_copy_unfused:
    movups  s(%rip), %xmm0
    movups  %xmm0, (%rdx)

    movq    (%rdx), %rax
    movq    %rax, (%rsi)
    movq    8(%rdx), %rax
    movq    %rax, 8(%rsi)

    decl    %edi
    jnz     init_fused_copy_unfused
    ret

.globl init_unfused_copy_unfused
.align 16
init_unfused_copy_unfused:
    leaq    c(%rip), %rax
    movq    %rax, (%rdx)
    leaq    i(%rip), %rax
    movq    %rax, 8(%rdx)

    movq    (%rdx), %rax
    movq    %rax, (%rsi)
    movq    8(%rdx), %rax
    movq    %rax, 8(%rsi)

    decl    %edi
    jnz     init_unfused_copy_unfused
    ret
)");

int main()
{
    using namespace std::chrono;

    S s1, s2;
    constexpr int N = 1'000'000'000;
    auto t1 = system_clock::now();
    init_fused_copy_fused(N, s2, s1);
    auto t2 = system_clock::now();
    init_fused_copy_unfused(N, s2, s1);
    auto t3 = system_clock::now();
    init_unfused_copy_fused(N, s2, s1);
    auto t4 = system_clock::now();
    init_unfused_copy_unfused(N, s2, s1);
    auto t5 = system_clock::now();

    std::cout
     << "init fused copy fused     " << duration_cast<duration<double>>(t2 - t1) << "\n"
     << "init fused copy unfused   " << duration_cast<duration<double>>(t3 - t2) << "\n"
     << "init unfused copy fused   " << duration_cast<duration<double>>(t4 - t3) << "\n"
     << "init unfused copy unfused " << duration_cast<duration<double>>(t5 - t4) << "\n";
}
