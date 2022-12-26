#include <type_traits>
#include <iostream>

// 如果你想看天书的话：
// https://en.cppreference.com/w/cpp/language/value_category


// 判断value categories的方法
// 改编自C++ Templates: The Complete Guide第二版，附录B
//
// e: expression

#define IS_LVALUE(e, type)  std::is_same<decltype((e)), type &>::value
#define IS_XVALUE(e, type)  std::is_same<decltype((e)), type &&>::value
#define IS_PRVALUE(e, type) std::is_same<decltype((e)), type>::value


/////////////////////////////


int func() { return 1; }

// uncopyable & unmovable
class Class {
public:
    Class() = default;
    Class(const Class&) = delete;
    Class(Class&&) = delete;
};



int main() {
    auto println = [](bool arg) {
        std::cout << (arg ? "true" : "false") << std::endl; 
    };

    println(IS_PRVALUE(1 + 2, int));
    println(IS_PRVALUE(Class{}, Class));
    println(IS_PRVALUE(func(), int));
    println(IS_PRVALUE(1, int));


    Class object;
    Class *pObject = &object;

    println(IS_LVALUE(object, Class));
    println(IS_LVALUE(*pObject, Class));
    using StringLiteral = const char[13];
    // Note: 在字面值类型中，字符串字面值是特殊的，并非prvalue
    println(IS_LVALUE("shabi xiaomi", StringLiteral));


    int eXpring = 1;

    println(IS_XVALUE(std::move(eXpring), int));
    // Note: prvalue-to-xvalue是C++17及之后后才允许的
    //       之前的C++标准会引起编译错误
    // println(STRONG_IS_XVALUE(std::move(Class{}), Class));
    return 0;
}