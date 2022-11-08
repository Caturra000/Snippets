// 示例来源：https://www.bilibili.com/video/BV1qG4y1n7QE

#include <sched.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <tuple>

void bindToCpu(int index) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(index, &set);
    sched_setaffinity(index, sizeof set, &set);
}

template <typename T, int i>
struct Unroll {
    static void doUnroll(T *totals, const T *a) {
        totals[i] += a[i];
        if constexpr (~i) {
            Unroll<T, i-1>::doUnroll(totals, a);
        }
    }
};

template <typename T, int N>
__attribute__((noinline))
T sum(const T *a, int count) {
    T totals[N] {};
    for(int i = 0; i < count; i += N) {
        Unroll<T, N-1>::doUnroll(totals, a + i);
    }
    T total = 0;
    for(int i = 0; i < N; ++i) {
        total += totals[i];
    }
    return total;
}

template <typename T, int N>
void test(const std::vector<T> &nums) {
    using namespace std::chrono;
    auto [elapsed, s] = [](auto &&func) {
        auto start = high_resolution_clock::now();
        auto ret = func();
        auto end = high_resolution_clock::now();
        return std::make_tuple(duration_cast<milliseconds>(end - start), ret);
    } ([&] {
        T s = 0;
        for(int i = 0; i < 1024 * 256; i++) {
            s += sum<T, N>(nums.data(), nums.size());
            asm volatile("": :"r,m"(s): "memory");
        }
        return s;
    });
    std::cout << s << '\t' << N << "\telapsed: " << elapsed.count() << "ms" << std::endl;
}

/////////////////////////////////////////

constexpr int N = 1024 * 4;
static std::vector<float> numsf(N);
static std::vector<int> numsi(N);

void prepare() {
    bindToCpu(0);
    for(int i = 0; i < N; i++) {
        numsi[i] = numsf[i] = rand();
    }
}

template<int N>
void testcase() {
    test<int, N>(numsi);
    test<float, N>(numsf);
    std::cout << std::endl;
}

template <int N, int Max>
void runTestcases() {
    testcase<N>();
    if constexpr(N < Max) {
        runTestcases<N+1, Max>();
    }
}

int main() {
    prepare();
    runTestcases<1, 8>();
    return 0;
}
