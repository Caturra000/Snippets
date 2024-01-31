#include <type_traits>
#include <vector>
#include <iostream>

// Part of std::invoke.
namespace standard {
   namespace detail {
      void tag_invoke();
      struct tag_invoke_t {
         template<typename Tag, typename... Args>
         constexpr decltype(auto) operator() (Tag tag, Args &&... args) const /*noexcept(...)*/ {
            // Internal ADL!
            return tag_invoke(static_cast<Tag &&>(tag), static_cast<Args &&>(args)...);
         }
      };
   }

   inline constexpr detail::tag_invoke_t tag_invoke{};

   template<auto& Tag>
   using tag_t = std::decay_t<decltype(Tag)>;
}

namespace library {

    namespace begin_cpo_detail {

        struct Begin_fn {
            template <typename /*👈concept is better*/ Container>
            decltype(auto) operator()(Container &&container) const {
                return standard::tag_invoke(*this, std::forward<Container>(container));
            }
        };

        // Generic default begin function template(s).
        // TODO: array types...
        template <typename T>
        decltype(auto) tag_invoke(Begin_fn, T &&obj) /*noexcept(noexcept(...))*/ {
            return std::forward<T>(obj).begin();
        }
    }

    inline namespace _cpo {
        inline constexpr begin_cpo_detail::Begin_fn begin {};
    } // _cpo
} // library


namespace end_user {

template <typename T>
class Vector {
public:
    Vector(std::initializer_list<T> il): _real_vector(il) {}
    // Library-defined begin() function.
    auto begin() { return _real_vector.begin(); }

private:
    std::vector<T> _real_vector;

    // User-defined hook.
    // A hooked begin-iterator (with hidden friend again).
    friend auto tag_invoke(standard::tag_t<library::begin>, Vector<T> &vec) {
        using Hook = std::vector<std::string>;
        static Hook strings {"hook", "jintianxiaomidaobilema"};
        return strings.begin();
    }
};

} // end_user


int main() {
    end_user::Vector<int> f { 1, 2, 3 };
    auto iter1 = f.begin();
    // [1]
    std::cout << *iter1 << std::endl;

    ::puts("===============");

    // STL-begin.
    auto iter2 = std::begin(f);
    // [1]
    std::cout << *iter2 << std::endl;

    ::puts("===============");

    // No ADL enabled.
    // auto iter3 = begin(f);
    //
    // ::puts("===============");

    // **tag_invoke is here.**
    // ADL disabled, since library::begin is an object.
    //
    // With library:: prefix, not end_user::,
    // `library` can detect user-defined hooks automatically.
    auto iter4 = library::begin(f);
    // [hook]
    std::cout << *iter4 << std::endl;
}
