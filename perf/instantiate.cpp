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

template <auto N>
using m = std::make_index_sequence<N>;

template <auto First, auto Last>
constexpr auto constexpr_for = [](auto &&f) {
    [&]<auto I>(this auto &&self) {
        if constexpr (I < Last) {
            f.template operator()<I>();
            self.template operator()<I + 1>();
        }
    }.template operator()<First>();
};

constexpr int retries = 10;

// 每种 (N,E) 组合实例化 8 个 make_index_sequence
template <int N, typename E>
struct heavy_op {
    using type = std::tuple<
        E,
        m<N * 1>,
        m<N * 2>,
        m<N * 3>,
        m<N * 4>,
        m<N * 5>,
        m<N * 6>,
        m<N * 7>,
        m<N * 8>
    >;
    static constexpr int value = N;
};

template <int I>
struct env {
    static constexpr int layer = I;
};

struct dummy_tag {};

template <int N, typename Tag>
consteval int compute_impl() {
    typename heavy_op<N, Tag>::type t;
    [[maybe_unused]] auto sz = sizeof(t);
    if constexpr (N > 0) {
        return heavy_op<N, Tag>::value + compute_impl<N - 1, Tag>();
    }
    return heavy_op<0, Tag>::value;
}

// 无缓存：Tag = E
template <int N, typename E>
consteval int compute_no_cache(int) {
    return compute_impl<N, E>();
}

// 有缓存：Tag = dummy_tag，结果缓存到变量模板
template <int N>
constexpr auto cached = compute_impl<N, dummy_tag>();

template <int N, typename E>
consteval int compute_with_cache(int) {
    return cached<N>;
}

template <int D>
consteval int chain_no_cache() {
    int s = 0;
    constexpr_for<0, retries>([&s]<auto i> {
        s += compute_no_cache<D, env<i>>(0);
    });
    return s;
}

template <int D>
consteval int chain_with_cache() {
    int s = 0;
    constexpr_for<0, retries>([&s]<auto i> {
        s += compute_with_cache<D, env<i>>(0);
    });
    return s;
}

int main() {
    int s = 0;
    constexpr_for<61, 120>([&s]<auto n> {
        #ifdef NOCACHE
        s += chain_no_cache<n>();
        #elifdef CACHE
        s += chain_with_cache<n>();
        #else
        #error "MUST define CACHE or NOCACHE!"
        #endif
    });
    return s;
}
