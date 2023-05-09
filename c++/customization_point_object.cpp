#include <type_traits>
#include <vector>
#include <iostream>

// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4381.html
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
            decltype(auto) operator()(Container &&container) {
                // !!unqualified name lookup
                return begin(std::forward<Container>(container));
            }
        };
    }

// A customization point OBJECT.
inline cpo_detail::Begin_Fn begin {};

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
    // A begin-iterator with log
    friend std::vector<int>::iterator begin(Vector<int> &vec) {
        std::cout << "log" << std::endl;
        return vec._real_vector.begin();
    }
};

} // end_user


int main() {
    end_user::Vector<int> f { 1, 2, 3 };
    auto iter1 = f.begin();
    // [1]
    std::cout << *iter1 << std::endl;

    ::puts("===============");

    // With framework:: prefix, not end_user::
    // `framework` can detect user-defined hook automatically
    auto iter2 = framework::begin(f);
    // [log, 1]
    std::cout << *iter2 << std::endl;
}
