#include <array>
#include <ranges>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <vector>
#include <format>

// C++23才支持zip，因此这里写了一个简单的C++20兼容版
// NOTE: 实际上并不支持view（比如vector | take(n)），因此又写了另一版，见zip_more_ranges.cpp
auto zip(auto &...containers) noexcept {
    return std::views::iota(size_t{}, std::min({std::size(containers)...}))
        |  std::views::transform([&](auto index) {
                return std::tuple<decltype(containers[index])&...>(containers[index]...);
            });
}

int main() {
    std::array arr1 {1, 2, 3, 4, 5, 6, 7};
    std::array arr2 {'a', 'b', 'c', 'd', 'e'};
    std::vector arr3 {"aha", "hola", "kokona"};

    for(auto [i, c, s] : zip(arr1, arr2, arr3)) {
        std::cout << std::format("[{}|{}|{}] ", i, c, s);
    }
    std::cout << std::endl;
}
