#include <iostream>
#include <tuple>
#include <ranges>

constexpr unsigned long CHUNK_SHIFT = 6;
constexpr bool verbose = true;
using I = unsigned long; // for index.
using O = unsigned int; // for order.


auto calc(I index, O order) {
    auto xa_index = order < 64 ? (index >> order) << order : 0;
    auto xa_shift = order - (order % CHUNK_SHIFT);
    auto xa_sibs = (1 << (order % CHUNK_SHIFT)) - 1;
    return std::tuple(xa_index, xa_shift, xa_sibs);
}

auto output(auto index, auto shift, auto sibs) {
    std::cout << "[" << index << ", " << shift << ", " << sibs << "]\n";
}

auto println(auto &&...v) {
    (std::cout << ... << v) << std::endl;
}

int main() {
    auto [index, shift, sibs] = calc(0,0);
    auto c = [&] { return [&](auto ...x) { return std::tie(index, shift, sibs) = calc(x...); }; };
    auto o = [&] { output(index, shift, sibs); };

    auto test = [&](std::tuple<I, I> indices, O order) {
        auto [start, end] = indices;
        if(verbose) println("test: index=[", start, ", ", end, "), order=", order);
        for(auto i : std::views::iota(start, end)) {
            c()(i, order);
            o();
        }
        println();
    };

    test({0, 18}, 3);
    test({56, 67}, 3);
    test({56, 67}, 6);
    test({56, 67}, 7);
    test({56, 67}, 8);
}
