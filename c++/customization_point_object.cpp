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

namespace framework {

    namespace cpo_detail {

        // Generic begin function templates
        template <typename T>
        decltype(auto) begin(T &&obj) {
            return std::forward<T>(obj).begin();
        }

        // TODO array...

        struct Begin_Fn {
            template <typename Container>
            decltype(auto) operator()(Container &&container) const {
                // !!unqualified name lookup
                return begin(std::forward<Container>(container));
            }
        };
    }

#if __cplusplus >= 201703L

// A customization point OBJECT.
inline constexpr cpo_detail::Begin_Fn begin {};

#else // __cplusplus >= 201703L

// workaroud for C++14

template <typename Cpo>
constexpr Cpo __static_const {};

namespace {
    // A customization point OBJECT.
    constexpr auto &begin = __static_const<cpo_detail::Begin_Fn>;
}

#endif

} // framework


namespace end_user {

template <typename T>
class Vector {
public:
    Vector(std::initializer_list<T> il): _real_vector(il) {}
    // Framework-defined begin() function
    auto begin() { return _real_vector.begin(); }
private:
    std::vector<T> _real_vector;


    // User-defined hook
    // A hooked begin-iterator
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

    // STL-begin
    auto iter2 = std::begin(f);
    // [1]
    std::cout << *iter2 << std::endl;

    ::puts("===============");

    // ADL enabled, STL-bypass-begin
    auto iter3 = begin(f);
    // [hook]
    std::cout << *iter3 << std::endl;

    ::puts("===============");

    // ADL disabled, since framework::begin is an object
    //
    // With framework:: prefix, not end_user::
    // `framework` can detect user-defined hook automatically
    auto iter4 = framework::begin(f);
    // [hook]
    std::cout << *iter4 << std::endl;
}
