#include <algorithm>
#include <array>
#include <print>
#include <ranges>
#include <random>
#include <chrono>
#include <tuple>
#include <cassert>
#include <experimental/simd>
#include <immintrin.h>

namespace stdx = std::experimental;
namespace stdv = std::views;
namespace stdr = std::ranges;

template <typename T, auto N>
constexpr auto simd_max_element(const std::array<T, N> &arr) {
    using simd_t = stdx::native_simd<T>;
    simd_t max_value = std::numeric_limits<T>::min();

    constexpr auto step = simd_t::size();
    constexpr auto tile = N / step;
    constexpr auto left = N % step;

    for(auto &batch : arr | stdv::stride(step) | stdv::take(tile)) {
        simd_t temp {std::addressof(batch), stdx::element_aligned};
        where(max_value < temp, max_value) = temp;
    }

    if constexpr (left) {
        auto left_view = arr | stdv::drop(tile * step);
        auto v = *stdr::max_element(left_view);
        if(stdx::none_of(max_value > v)) {
            return v;
        }
    }

    return stdx::hmax(max_value);
}

constexpr std::size_t MASS = 1e8;
std::array<int, MASS> massive;

// Generate random values and flush cachelines.
// Return a maximum value for assert().
auto initiate(auto &arr) {
    auto verifier = *begin(arr);
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, arr.size());
    for(auto &v : arr) {
        v = distribution(generator);
        verifier = std::max(v, verifier);
    }

    constexpr auto cacheline_elements = 64 / sizeof(arr[0]);
    auto flush_view = stdv::iota(arr.data())
        | stdv::stride(cacheline_elements)
        | stdv::take(arr.size() / cacheline_elements);
    for(auto addr : flush_view) {
        _mm_clflush(addr);
    }
    return verifier;
}

auto tick(auto f) {
    namespace stdc = std::chrono;
    auto start = stdc::steady_clock::now();
    auto v = f();
    auto end = stdc::steady_clock::now();
    auto elapsed = stdc::duration_cast<stdc::milliseconds>(end - start);
    return std::tuple(v, elapsed);
}

// clang++-18 -O3 -std=c++2b -march=skylake测试结果：
// * SIMD版本20ms
// * 标准库版本80ms
int main() {
    auto check = initiate(massive);
    auto [v, elapsed] = tick([&] { return simd_max_element(massive); });
    // auto [v, elapsed] = tick([&] { return *stdr::max_element(massive); });
    assert(v == check);
    std::println("max value: {}, elapsed: {}", v, elapsed);
}
