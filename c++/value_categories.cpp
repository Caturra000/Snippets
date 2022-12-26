#include <type_traits>
#include <iostream>

// e: expression

#define IS_LVALUE(e, type)  std::is_same_v<decltype((e)), type &>
#define IS_XVALUE(e, type)  std::is_same_v<decltype((e)), type &&>
#define IS_PRVALUE(e, type) std::is_same_v<decltype((e)), type>

#define STRONG_IS_LVALUE(e, type)  ( IS_LVALUE(e, type) && !IS_XVALUE(e, type) && !IS_PRVALUE(e, type))
#define STRONG_IS_XVALUE(e, type)  (!IS_LVALUE(e, type) &&  IS_XVALUE(e, type) && !IS_PRVALUE(e, type))
#define STRONG_IS_PRVALUE(e, type) (!IS_LVALUE(e, type) && !IS_XVALUE(e, type) &&  IS_PRVALUE(e, type))

int func() { return 1;}

class Class {
public:
    Class() = default;
    Class(const Class&)=delete;
    Class(Class&&)=delete;
};



int main() {
    auto println = [](bool arg) {
        std::cout << (arg ? "true" : "false") << std::endl; 
    };

    println(STRONG_IS_PRVALUE(1 + 2, int));
    println(STRONG_IS_PRVALUE(Class{}, Class));
    println(STRONG_IS_PRVALUE(func(), int));
    println(STRONG_IS_PRVALUE(1, int));


    Class object;
    Class *pObject = &object;

    println(STRONG_IS_LVALUE(object, Class));
    println(STRONG_IS_LVALUE(*pObject, Class));
    using StringLiteral = const char[13];
    println(STRONG_IS_LVALUE("shabi xiaomi", StringLiteral));


    int eXpring = 1;

    println(STRONG_IS_XVALUE(std::move(eXpring), int));
    // Note: prvalue-to-xvalue是C++17及之后后才允许的
    //       之前的C++标准会引起编译错误
    // println(STRONG_IS_XVALUE(std::move(Class{}), Class));
    return 0;
}