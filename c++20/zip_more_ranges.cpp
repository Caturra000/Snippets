#include <array>
#include <ranges>
#include <tuple>
#include <iostream>
#include <algorithm>
#include <vector>
#include <format>

namespace punipuni {

template <std::ranges::input_range ...Views>
class Zip_view : public std::ranges::view_interface<Zip_view<Views...>> {
public:
    struct iterator;
    struct sentinel;

public:
    Zip_view() = default;
    // Views are cheap to copy, but owning views cannot be done. (= delete)
    constexpr Zip_view(Views ...vs) noexcept: _views(std::move(vs)...) {}
    constexpr auto begin() {
        return std::apply([&](Views &...views) { return iterator(views...); }, _views);
    }
    constexpr auto end() requires (std::ranges::random_access_range<Views> && ...) {
        return sentinel{this};
    }
    constexpr auto size() const requires (std::ranges::sized_range<Views> && ...) {
        return std::apply([&](auto &&...views) { return std::min({std::ranges::size(views)...}); }, _views);
    }

private:
    std::tuple<Views...> _views;
};

template <std::ranges::input_range ...Views>
struct Zip_view<Views...>::iterator {
    friend struct sentinel;
    // TODO: flexible iterator_concepts.
    using iterator_concept = std::random_access_iterator_tag;
    using iterator_category = std::input_iterator_tag;
    using value_type = std::tuple<std::ranges::range_value_t<Views>...>;
    using difference_type = std::common_type_t<std::ranges::range_difference_t<Views>...>;

    iterator() = default;
    constexpr iterator(Views &...views): _currents{std::ranges::begin(views)...} {}

    constexpr auto operator*() const {
        return std::apply([&](auto &&...iters) {
            // No <auto> decay!
            // Example: zip(views::iota(1, 5), named_vector_of_int).
            // Return: std::tuple<int, int&>.
            return std::tuple<decltype(*iters)...>((*iters)...);
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

    friend constexpr auto operator<=>(const iterator &x, const iterator &y) = default;

private:
    std::tuple<std::ranges::iterator_t<Views>...> _currents;
};

template <std::ranges::input_range ...Views>
struct Zip_view<Views...>::sentinel {
    sentinel() = default;
    constexpr sentinel(Zip_view *this_zip) noexcept: _this_zip(this_zip) {}

    friend bool operator==(const iterator &x, const sentinel &y) {
        return [&]<auto ...Is>(std::index_sequence<Is...>) {
            return ((std::get<Is>(x._currents) == std::ranges::end(std::get<Is>(y._this_zip->_views))) || ...); 
        }(std::make_index_sequence<sizeof...(Views)>{});
    }

private:
    Zip_view *_this_zip;
};

inline constexpr struct Zip_fn {
    template <std::ranges::input_range ...Rs>
    [[nodiscard]]
    constexpr auto operator()(Rs &&...rs) const {
        if constexpr (sizeof...(rs) == 0) {
            return std::views::empty<std::tuple<>>;
        } else {
            return Zip_view<std::views::all_t<Rs>...>(std::forward<Rs>(rs)...);
        }
    }
} zip;

} // namespace punipuni

int main() {
    std::vector vec1 {"Ave", "Mujica", "Haruhikage"};
    std::array arr2 {'a', 'b', 'c', 'd', 'e'};
    std::array arr3 {1, 2, 3, 4, 5, 6, 7};
    using namespace std::literals;
    std::vector eXpiring {"Chino"s, "Cocoa"s, "Rize"s, "Syaro"s, "Chiya"s};

    for(auto [s, c, i, A, X] : punipuni::zip(vec1,
                                          arr2,
                                          arr3 | std::views::drop(1),
                                          std::views::iota('A', 'Z'),
                                          std::move(eXpiring))) {
        std::cout << std::format("[{}|{}|{}|{}|{}] ", s, c, i, A, X);
    }
    std::cout << std::endl;
}
