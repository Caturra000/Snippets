// [WORK IN PROGRESS]
//
// stdexec 问题:
//   每层 let_value 传入不同 Env，即使 Sender 相同也要重复计算。
//   如果核心计算不依赖 Env，可将 O(Layers×Senders) 降为 O(Senders)。
//
// 本 demo 做简单的最小复现。
//
// 编译:
//   time clang++-20 -std=c++23 -c instantiate.cpp -DNOCACHE -o /dev/null
//   time clang++-20 -std=c++23 -c instantiate.cpp -DCACHE   -o /dev/null

#include <tuple>
#include <utility>
#include <iostream>

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            self.template operator()<I + 1>();
        }
    }.template operator()<First>();
};

constexpr int retries = 32;

template <int N, typename Env>
struct op {
    using type = Env;
    static constexpr int value = N;
};

template <int I>
struct env {
    static constexpr int layer = I;
};

struct fixed_tag {};

template <int N, typename Tag>
consteval int compute_impl() {
    if constexpr (N > 0) {
        return op<N, Tag>::value + compute_impl<N - 1, Tag>();
    }
    return op<0, Tag>::value;
}

// 无缓存：tag = 每次单独计算的 env
template <int N, typename>
consteval int compute_without_cache(int) {
    using real_env = decltype([]{});
    return compute_impl<N, real_env>();
}

// 有缓存：需要通过某种方式，使得类型实例化前抛掉 E
// 具体做法我还没想好，但是目前可以看到编译性能差距
template <int N, typename>
consteval int compute_with_cache(int) {
    return compute_impl<N, fixed_tag>();
}

int main() {
    int s = 0;
    constexpr int first = 60;
    constexpr int last = 120;
    constexpr_for<first, last>([&s]<auto n> {
        constexpr_for<0, retries>([&s]<auto i> {
            #ifdef NOCACHE
            s += compute_without_cache<n, env<i>>(0);
            #elifdef CACHE
            s += compute_with_cache<n, env<i>>(0);
            #else
            #error "MUST define CACHE or NOCACHE!"
            #endif
        });
    });
    std::cout << s << std::endl;
    return 0;
}
