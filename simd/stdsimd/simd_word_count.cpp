#include <algorithm>
#include <print>
#include <ranges>
#include <random>
#include <chrono>
#include <cassert>
#include <string>
#include <iostream>
#include <bitset>

// 使用了TS版本不支持的函数，需要引入第三方库
// （到C++26支持std::simd就不需要了）
#include <vir/simd.h>
#include <vir/simd_bit.h>
#include <vir/simd_bitset.h>

namespace stdx = vir::stdx;
namespace stdv = std::views;
namespace stdr = std::ranges;

std::string_view frost = "Whose woods these are I think I know.\n"
                         "His house is in the village though;  \n"
                         "He will not see me stopping here     \n"
                         "To watch his woods fill up with snow.\n";

namespace detail {

auto isspace(auto c /* or simd_chars */) {
    return c == ' ' || c == '\n';
}

std::size_t std_nvda_word_count(stdr::view auto s) {
    if(!std::size(s)) return 0;
    return std::transform_reduce(std::begin(s), std::prev(std::end(s)),
                                 std::next(std::begin(s)),
                                 std::size_t(!isspace(s.front())),
                                 std::plus<>(),
                                 [](auto l, auto r) {
                                     return isspace(l) && !isspace(r);
                                 });
}

std::size_t simd_word_count(stdr::view auto s) {
    using simd_t = stdx::native_simd<char>;
    constexpr auto step = simd_t::size();
    const auto tile = std::size(s) / step;

    auto simdify = stdv::stride(step) | stdv::take(tile);
    std::size_t count = 0;

    //////////////////////////////////////////////////

    auto simd_view = s | simdify;
    auto simd_transform = [&](auto &to_simd) {
        simd_t temp (std::addressof(to_simd), stdx::element_aligned);
        auto mask = isspace(temp);
        auto bits = vir::to_bitset(mask);
        return (bits & (~(bits) << 1)).count();
    };
    for(auto &&to_simd : simd_view) {
        count += simd_transform(to_simd);
    }

    //////////////////////////////////////////////////

    auto x = s | simdify | stdv::drop(1);
    auto y = s | stdv::drop(step - 1) | simdify;
    auto z = stdv::zip(x, y);
    for(auto [curr, prev] : z) {
        count += isspace(curr) && !isspace(prev);
    }

    //////////////////////////////////////////////////

    if(auto batch = step * tile; std::size(s) != batch) {
        auto left_view = s | stdv::drop(batch);
        auto prev_view = s | stdv::drop(batch - 1);
        count += std_nvda_word_count(left_view);
        count += isspace(left_view[0]) && !isspace(prev_view[0]);
    }
    return count;
}

} // namespace detail

template <typename T>
concept string_class = std::is_convertible_v<T, std::string_view>;

std::size_t std_nvda_word_count(string_class auto &&str) {
    return detail::std_nvda_word_count(str | stdv::all);
}

std::size_t simd_word_count(string_class auto &&str) {
    return detail::simd_word_count(str | stdv::all);
}

auto tick(auto f) {
    namespace stdc = std::chrono;
    auto start = stdc::steady_clock::now();
    auto v = f();
    auto end = stdc::steady_clock::now();
    auto elapsed = stdc::duration_cast<stdc::milliseconds>(end - start);
    return std::tuple(v, elapsed);
}

// 使用g++-14 -O3 -march=skylake -std=c++2b编译
// 并使用文本生成器测试1e9规模
// * SIMD版本149ms
// * 标准库版本776ms
int main() {
    std::string text;
    for(std::string line; std::getline(std::cin, line);) {
        if(!text.empty()) text += ' ';
        text += line;
    }

    auto [count, elapsed] = tick([&] { return simd_word_count(text); });
    // auto [count, elapsed] = tick([&] { return std_nvda_word_count(text); });

    std::println("count: {}, time: {}", count, elapsed);
}
