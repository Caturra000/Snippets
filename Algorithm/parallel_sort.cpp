#include <algorithm>
#include <array>
#include <print>
#include <ranges>
#include <random>
#include <chrono>
#include <tuple>
#include <cassert>
#include <thread>
#include <immintrin.h>

namespace stdv = std::views;
namespace stdr = std::ranges;

template <std::size_t N>
concept simd_sortable = std::has_single_bit(N);

// Bitonic sort.
namespace detail {

auto schedule(std::size_t hint, std::invocable auto job) {
    if(hint > 1e6) return std::jthread{std::move(job)};
    return job(), std::jthread{};
}

// Sort up and sort down.
void make_first_bitonic(stdr::random_access_range auto &&range) {
    auto pivot = begin(range) + size(range) / 2;
    auto up = stdr::subrange(begin(range), pivot);
    auto down = stdr::subrange(pivot, end(range));
    // These can be done in parallel.
    auto _ = schedule(std::size(range),
        [&]{ stdr::sort(up); });
    stdr::sort(down, std::greater{});
}

// Merge only.
void bitonic_sort(stdr::random_access_range auto &&range) {
    if(std::size(range) == 1) return;
    auto pivot = std::begin(range) + std::size(range) / 2;
    auto left = stdr::subrange{std::begin(range), pivot};
    auto right = stdr::subrange{pivot, std::end(range)};
    for(auto iters : stdv::zip(left, right)) {
        if(auto &&[i1, i2] = iters; i1 > i2) {
            std::swap(i1, i2);
        }
    }
    auto hint = std::size(range);
    auto _ = schedule(hint,
        [&] {bitonic_sort(stdr::subrange(std::begin(range), pivot));});
    bitonic_sort(stdr::subrange(pivot, std::end(range)));
}

} // namespace detail

void simd_sort(stdr::random_access_range auto &&range) {
    if(size(range) < 2) return;
    assert(std::has_single_bit(size(range)));
    detail::make_first_bitonic(range | stdv::all);
    detail::bitonic_sort(range | stdv::all);
}

constexpr std::size_t MASS = 1<<26;
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
// * 并行版本：2630ms
// * 标准库版本：4972ms
int main() {
    initiate(massive);
    // bitonic sort要求容器大小为2的幂
    static_assert(std::has_single_bit(std::size(massive)));
    auto e = tick([&]{ simd_sort(massive); });
    // auto e = tick([&] { stdr::sort(massive); });
    assert(std::ranges::is_sorted(massive));
    std::println("time: {}", e);
}
