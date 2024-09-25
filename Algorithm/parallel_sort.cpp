#include <algorithm>
#include <array>
#include <print>
#include <ranges>
#include <random>
#include <chrono>
#include <tuple>
#include <cassert>
#include <thread>
#include <vector>
#include <immintrin.h>

namespace stdv = std::views;
namespace stdr = std::ranges;

namespace bitonic {

//////////////////////////////////////////////////////////// parallel

// A rough tuning option for amortizing thread overheads.
constexpr std::size_t default_scale_hint = 1 << 20;
// We don't have a good thread pool here, so let it be false.
constexpr bool enable_parallel_for = false;
constexpr bool enable_parallel(std::integral auto hint) {
    return hint >= default_scale_hint;
}

// Note: parallel(...) and parallel(...) are not parallel!
void parallel(std::size_t scale_hint, std::invocable auto job,
                                      std::invocable auto ...jobs) {
    if(!enable_parallel(scale_hint)) return job(), (jobs(), ...);
    std::array parallel_jobs {std::jthread{std::move(jobs)}...};
    job();
}

void parallel_for(auto view, auto func) {
    auto f = [func](auto maybe_subview) {
        for(auto &&v : maybe_subview) {
            func(std::forward<decltype(v)>(v));
        }
    };
    if(!enable_parallel_for || !enable_parallel(std::size(view))) {
        return f(view);
    }
    auto per_thread_view = view | stdv::chunk(default_scale_hint);
    std::vector<std::jthread> parallel_jobs;
    for(auto g : per_thread_view) {
        parallel_jobs.emplace_back(f, g);
    }
}

//////////////////////////////////////////////////////////// parallel (end)
//////////////////////////////////////////////////////////// misc

constexpr struct {
    std::less<>    incr;
    std::greater<> decr;
} direction;

template <typename dir>
concept directional =
    std::is_same_v<dir, decltype(direction.incr)> ||
    std::is_same_v<dir, decltype(direction.decr)>;

void merge(auto range, directional auto dir);
void sort(auto range, directional auto dir);

auto cut(auto range) {
    auto pivot = std::begin(range) + std::size(range) / 2;
    auto lo = stdr::subrange(std::begin(range), pivot);
    auto hi = stdr::subrange(pivot, std::end(range));
    return std::tuple(lo, hi);
}

void perform(auto &&bitonic_pair, directional auto dir) {
    auto &&[lhs, rhs] = bitonic_pair;
    if(!dir(lhs, rhs)) {
        std::swap(lhs, rhs);
    }
}

//////////////////////////////////////////////////////////// misc (end)
//////////////////////////////////////////////////////////// core

void sort(auto range, directional auto dir) {
    if(std::size(range) < 2) return;
    auto [lo, hi] = cut(range);
    parallel(std::size(range),
        [=]{ sort(lo, direction.incr); },
        [=]{ sort(hi, direction.decr); });
    merge(range, dir);
}

void merge(auto range, directional auto dir) {
    if(std::size(range) < 2) return;

    auto [lo, hi] = cut(range);
    auto zip_view = stdv::zip(lo, hi);

    // All the comparisons can be done in parallel.
    parallel_for(zip_view,
        [=](auto &&zipped) {
            perform(zipped, dir);
        });

    parallel(std::size(range),
        [=]{ merge(lo, dir); },
        [=]{ merge(hi, dir); });
}

} // namespace bitonic

void parallel_sort(stdr::random_access_range auto &&range) {
    if(std::size(range) < 2) return;
    assert(std::has_single_bit(std::size(range)));
    bitonic::sort(range | stdv::all, bitonic::direction.incr);
}

//////////////////////////////////////////////////////////// core (end)

constexpr std::size_t MASS = 1 << 26;
std::array<int, MASS> massive;

// Generate random values and flush cachelines.
void initiate(auto &arr) {
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, arr.size());
    for(auto &v : arr) {
        v = distribution(generator);
    }

    constexpr auto cacheline_elements = 64 / sizeof(arr[0]);
    auto flush_view = stdv::iota(arr.data())
        | stdv::stride(cacheline_elements)
        | stdv::take(arr.size() / cacheline_elements);
    for(auto addr : flush_view) {
        _mm_clflush(addr);
    }
}

auto tick(auto f) {
    namespace stdc = std::chrono;
    auto start = stdc::steady_clock::now();
    f();
    auto end = stdc::steady_clock::now();
    auto elapsed = stdc::duration_cast<stdc::milliseconds>(end - start);
    return elapsed;
}

// clang++-18 -O3，6e7数组测试：
// * 并行版本：2600ms
// * 标准库版本：4972ms
int main() {
    initiate(massive);
    // bitonic sort要求容器大小为2的幂
    static_assert(std::has_single_bit(std::size(massive)));
    auto elapsed = tick([&]{ parallel_sort(massive); });
    // auto elapsed = tick([&] { stdr::sort(massive); });
    assert(std::ranges::is_sorted(massive));
    std::println("time: {}", elapsed);
}
