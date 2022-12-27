#include <type_traits>
#include <iostream>

// 如果你想看天书的话：
// https://en.cppreference.com/w/cpp/language/value_category


// 判断value categories的方法
// 改编自C++ Templates: The Complete Guide第二版，附录B
//
// e: expression

#define IS_LVALUE(e)  std::is_lvalue_reference<decltype((e))>::value
#define IS_XVALUE(e)  std::is_rvalue_reference<decltype((e))>::value
#define IS_PRVALUE(e) (!IS_LVALUE(e) && !IS_XVALUE(e))


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

    println(IS_PRVALUE(1 + 2));
    println(IS_PRVALUE(Class{}));
    println(IS_PRVALUE(func()));
    println(IS_PRVALUE(1));
    // println(test_template_parameter_prvalue<1>());

    Class object;
    Class *pObject = &object;

    println(IS_LVALUE(object));
    println(IS_LVALUE(*pObject));
    // Note: 在字面值类型中，字符串字面值是特殊的，并非prvalue
    println(IS_LVALUE("shabi xiaomi"));


    int eXpring = 1;

    println(IS_XVALUE(std::move(eXpring)));
    // Question: prvalue-to-xvalue应该是C++17及之后后才允许的
    //           然而这里使用-std=c++11仍然可以编译
    println(IS_XVALUE(std::move(Class{})));
    return 0;
}