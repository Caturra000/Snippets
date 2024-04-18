#include <array>
#include <ranges>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <vector>
#include <format>

template <std::ranges::input_range ...Views>
class Zip_view : public std::ranges::view_interface<Zip_view<Views...>> {
/// Nested classes.
public:
    struct iterator;
    struct sentinel; // TODO

public:
    Zip_view() = default;
    Zip_view(Views ...vs): _views(vs...) {}
    constexpr auto begin() {
        return std::apply([&](Views &...views) { return iterator(views...); }, _views);
    }
    constexpr auto end() requires (std::ranges::random_access_range<Views> && ...) {
        return begin()[size()];
    }
    constexpr auto size() const requires (std::ranges::sized_range<Views> && ...) {
        return std::apply([&](auto &&...views) { return std::min({std::ranges::size(views)...}); }, _views);
    }

private:
    std::tuple<Views...> _views;
};

template <std::ranges::input_range ...Views>
struct Zip_view<Views...>::iterator {
    // TODO: flexible iterator_concepts.
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type = std::tuple<std::ranges::range_value_t<Views>...>;
    using difference_type = std::common_type_t<std::ranges::range_difference_t<Views>...>;

    iterator() = default;
    constexpr iterator(Views ...views): _currents{std::ranges::begin(views)...} {}
    // constexpr iterator(Zip_view<Views...> this_zip,Views ...views, difference_type n)
    //     : _this_zip(this_zip), _currents{(std::ranges::begin(views) + n)...} {}

    constexpr decltype(auto) operator*() {
        return std::apply([&](auto &&...iters) {
            return std::tuple<std::ranges::range_value_t<Views>&...>((*iters)...);
        }, _currents);
    }

    constexpr auto operator[](difference_type n) const {
        // return std::apply([&](auto &...views) {
        //     // WARN: operator--?
        //     // return iterator(_this_zip, (views | std::ranges::drop(n))...);
        // }, _views);
        auto tmp = *this;
        tmp.operator+=(n);
        return tmp;
    }

    constexpr iterator& operator++() {
        return this->operator+=(1);
    }

    constexpr iterator operator++(int) {
        auto tmp = *this;
        this->operator+=(1);
        return tmp;
    }
    constexpr iterator& operator+=(difference_type n) {
        std::apply([&](auto &...iters) { ((iters += n),...); }, _currents);
        return *this;
    }

    friend constexpr bool operator<=>(const iterator &x, const iterator &y) = default;

private:
    // Zip_view<Views...> *_this_zip;
    std::tuple<std::ranges::iterator_t<Views>...> _currents;
};

inline constexpr struct Zip_fn {
    template <std::ranges::input_range ...Rs>
    [[nodiscard]]
    constexpr auto operator()(Rs &&...rs) const {
        if constexpr (sizeof...(rs) == 0) {
            return std::views::empty<std::tuple<>>;
        } else {
            return Zip_view<std::views::all_t<decltype((rs))>...>(rs...);
        }
    }
} zip;

int main() {
    std::array arr1 {1, 2, 3, 4, 5, 6, 7};
    std::array arr2 {'a', 'b', 'c', 'd', 'e'};
    std::vector arr3 {"aha", "hola", "kokona"};

    for(auto [i, c, s] : zip(arr1, arr2, arr3 | std::views::drop(1))) {
        std::cout << std::format("[{}|{}|{}] ", i, c, s);
    }
    std::cout << std::endl;
}