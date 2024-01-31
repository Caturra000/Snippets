#include <type_traits>
#include <vector>
#include <iostream>

// CPO的介绍推荐看这一篇，非常精彩：
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4381.html
//
// 经典ADL用法虽然有高可扩展的能力，但是存在2个问题：
// 1. 不使用限定词时，出错难以发现
// 2. concept约束会被跳过，对于库开发并不友好
//
// 而CPO则通过在接口层面禁用ADL克服了这两个大难题
// 当然，它本身比较抽象，因此我在这里写了个demo

// TODO 这个示例还没有concept对比演示，有空加上去
// TODO tag_invoke


namespace library {

    // Best practice: provide a separate namespace for each CP class.
    namespace begin_cpo_detail {
        // Generic default begin function template(s).
        // TODO: array types...
        template <typename T>
        decltype(auto) begin(T &&obj) /*noexcept(noexcept(...))*/ {
            return std::forward<T>(obj).begin();
        }

        struct Begin_fn {
            template <typename /*👈concept is better*/ Container>
            decltype(auto) operator()(Container &&container) const {
                // Internal ADL here!
                return begin(std::forward<Container>(container));
            }
        };
    }

    // All CPOs should be wrapped into this inline namespace.
    inline namespace _cpo {
#if __cplusplus >= 201703L
        // A customization point OBJECT.
        inline constexpr begin_cpo_detail::Begin_fn begin {};
#else // __cplusplus >= 201703L
        // Workaround for C++14.
        template <typename Cpo>
        constexpr Cpo _static_const {};

        namespace {
            // A customization point "OBJECT".
            constexpr auto &begin = _static_const<begin_cpo_detail::Begin_fn>;
        }
#endif
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
    friend auto begin(Vector<T> &vec) {
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

    // ADL enabled, STL-bypass-begin.
    auto iter3 = begin(f);
    // [hook]
    std::cout << *iter3 << std::endl;

    ::puts("===============");

    // **CPO is here.**
    // ADL disabled, since library::begin is an object.
    //
    // With library:: prefix, not end_user::,
    // `library` can detect user-defined hooks automatically.
    auto iter4 = library::begin(f);
    // [hook]
    std::cout << *iter4 << std::endl;
}
