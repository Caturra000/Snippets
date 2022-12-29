#include <iostream>
class C {
public:
    C() = default;
    template <typename T>
    C(const T&) { std::cout << "const T&" << std::endl; }
    // C(const C&) = delete;
};

template <>
inline C::C<C>(const C&) { std::cout << "template <>" << std::endl; }

int main() {
    C x;
    // 无论如何都是predefined copy constructor
    // 而不是template
    // 如果禁用了C(const C&)反而会无法编译
    C y {x};
}
