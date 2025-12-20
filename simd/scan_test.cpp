#include "scan.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <tuple>

//////////////////////////////////////////////////////////// For test.

auto initiate(auto &arr) {
    std::default_random_engine generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 100); // no overflow
    for(auto &v : arr) {
        v = distribution(generator);
    }
    auto result = std::ranges::fold_left(arr, 0, std::plus());
    constexpr auto cacheline_elements = 64 / sizeof(arr[0]);
    auto flush_view = std::views::iota(arr.data())
        | std::views::stride(cacheline_elements)
        | std::views::take(arr.size() / cacheline_elements);
    for(auto addr : flush_view) {
        _mm_clflush(addr);
    }
    return result;
}

auto tick(auto f) {
    namespace stdc = std::chrono;
    auto start = stdc::steady_clock::now();
    auto v = f();
    auto end = stdc::steady_clock::now();
    auto elapsed = stdc::duration_cast<stdc::milliseconds>(end - start);
    return std::tuple(v, elapsed);
}

////////////////////////////////////////////////////////////

int main() {
    for(auto size : std::views::iota(1, 10000)) {
        std::vector<int> test_data(size);
        auto check = initiate(test_data);
        auto test_data_copy = test_data;
        auto [v, elapsed] = tick([&] { return scan_ilp(test_data); });
        std::inclusive_scan(test_data_copy.begin(),
                            test_data_copy.end(),
                            test_data_copy.begin(),
                            std::plus(), 0);
        for(auto i = 0; i < test_data.size(); ++i) {
            assert(test_data[i] == test_data_copy[i]);
        }
    }
    return 0;
}
