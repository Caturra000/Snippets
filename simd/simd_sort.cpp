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
#include <experimental/simd>
#include <immintrin.h>

namespace stdx = std::experimental;
namespace stdv = std::views;
namespace stdr = std::ranges;

namespace bitonic {

template <typename> struct use_simd {};

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

template <typename simd_t>
auto parallel_for(use_simd<simd_t>, auto view, auto func) {
    constexpr auto step = simd_t::size();
    const auto tile = std::size(view) / step;
    const auto consumed = step * tile;
    auto simdify = stdv::stride(step) | stdv::take(tile);
    parallel_for(view | simdify, std::move(func));
    return consumed;
}

//////////////////////////////////////////////////////////// parallel (end)
//////////////////////////////////////////////////////////// misc

constexpr struct {
    decltype([](){}) incr;
    decltype([](){}) decr;
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

void perform(auto &lhs, auto &rhs, directional auto dir) {
    auto compare = [dir] {
        if constexpr (dir == direction.incr) return std::less<>();
        else return std::greater<>();
    };
    if(!compare()(lhs, rhs)) {
        std::swap(lhs, rhs);
    }
}

template <typename simd_t>
void perform(use_simd<simd_t>, auto &lhs, auto &rhs, directional auto dir) {
    simd_t x {std::addressof(lhs), stdx::element_aligned};
    simd_t y {std::addressof(rhs), stdx::element_aligned};
    std::tie(x, y) = stdx::minmax(x, y);
    if constexpr (dir == direction.incr) {
        x.copy_to(std::addressof(lhs), stdx::element_aligned);
        y.copy_to(std::addressof(rhs), stdx::element_aligned);
    } else {
        x.copy_to(std::addressof(rhs), stdx::element_aligned);
        y.copy_to(std::addressof(lhs), stdx::element_aligned);
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
    using T = stdr::range_value_t<decltype(range)>;
    using simd_t = stdx::native_simd<T>;

    auto [lo, hi] = cut(range);
    auto zip_view = stdv::zip(lo, hi);

    // All the comparisons can be done in parallel.
    auto consumed =
    parallel_for(use_simd<simd_t>(), zip_view,
        [=](auto &&zipped) {
            auto &&[lhs, rhs] = zipped;
            perform(use_simd<simd_t>(), lhs, rhs, dir);
        });

    stdr::for_each(zip_view | stdv::drop(consumed),
        [=](auto &&zipped) {
            auto &&[lhs, rhs] = zipped;
            perform(lhs, rhs, dir);
        });

    parallel(std::size(range),
        [=]{ merge(lo, dir); },
        [=]{ merge(hi, dir); });
}

} // namespace bitonic

void simd_sort(stdr::random_access_range auto &&range) {
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
// * SIMD版本：2815ms
// * 标准库版本：4972ms
int main() {
    initiate(massive);
    using simd_t = stdx::native_simd<decltype(massive)::value_type>;
    // bitonic sort要求容器大小为2的幂
    static_assert(std::has_single_bit(std::size(massive)));
    auto elapsed = tick([&]{ simd_sort(massive); });
    // auto elapsed = tick([&] { stdr::sort(massive); });
    assert(std::ranges::is_sorted(massive));
    std::println("time: {}", elapsed);
}
