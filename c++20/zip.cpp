#include <array>
#include <ranges>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <vector>
#include <format>

// C++23才支持zip，因此这里写了一个简单的C++20兼容版
auto zip(auto &...containers) noexcept {
    return std::views::iota(size_t{}, std::min({std::size(containers)...}))
        |  std::views::transform([&](auto index) {
                return std::tuple(std::ref(containers[index])...);
            });
}

int main() {
    std::array arr1 {1, 2, 3, 4, 5, 6, 7};
    std::array arr2 {'a', 'b', 'c', 'd', 'e'};
    std::vector arr3 {"aha", "hola", "kokona"};

    // NOTE: 这里结构化绑定的都是reference_wrapper形式
    for(auto [i, c, s] : zip(arr1, arr2, arr3)) {
        // 似乎fmt对封装类的支持不怎样
        std::cout << std::format("[{}|{}|{}] ", i.get(), c.get(), s.get());
    }
    std::cout << std::endl;
}
