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
    // See xas_max().
    auto xa_max = xa_index;
    if(xa_shift || xa_sibs) {
        auto mask = ((xa_sibs + 1) << xa_shift) - 1;
        xa_max |= mask;
        if(xa_max == mask) xa_max++;
    }
    return std::tuple(xa_index, xa_shift, xa_sibs, xa_max);
}

auto output(auto index, auto shift, auto sibs, auto max) {
    std::cout << "[" << index << ", " << shift << ", " << sibs << "]";
    if(verbose) std::cout << " m: " << max;
    std::cout << std::endl;
}

auto println(auto &&...v) {
    (std::cout << ... << v) << std::endl;
}

int main() {
    auto [index, shift, sibs, max] = calc(0,0);
    auto c = [&] { return [&](auto ...x) { return std::tie(index, shift, sibs, max) = calc(x...); }; };
    auto o = [&] { output(index, shift, sibs, max); };

    auto test = [&](std::tuple<I, I> indices, O order) {
        auto [start, end] = indices;
        println("test: index=[", start, ", ", end, "), order=", order);
        for(auto i : std::views::iota(start, end)) {
            c()(i, order);
            o();
        }
        println();
    };

    std::tuple z {0, 18}; // 0 ~ 8 ~ 16 ~
    std::tuple i {56, 78}; // ~ 64 ~

    test(z, 3);

    test(i, 3);
    test(i, 6);
    test(i, 7);
    test(i, 8);

    test(i, 11);
    test(i, 12);
}
