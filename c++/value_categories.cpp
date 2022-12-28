#include <type_traits>
#include <iostream>

// 如果你想看天书的话：
// https://en.cppreference.com/w/cpp/language/value_category


// 判断value categories的方法
// 改编自C++ Templates: The Complete Guide第二版，附录B
//
// 关于e（既expression）为什么要用decltype((e))而不是decltype(e)
//
// §15.10.2
// If e is the name of an entity (such as variable, function, enumerator, or data member) or a class member access,
// decltype(e) yields the declared type of that entity or the denoted class member.
// Thus, decltype can be used to inspect the type of variable.
//
// §B.3
// The double parentheses in decltype((x)) are needed to avoid producing the declared type of a
// named entity in case where the expression x does indeed name an entity
// (in other cases, the parentheses have no effect).

#define IS_LVALUE(e)  std::is_lvalue_reference<decltype((e))>::value
#define IS_XVALUE(e)  std::is_rvalue_reference<decltype((e))>::value
#define IS_PRVALUE(e) (!IS_LVALUE(e) && !IS_XVALUE(e))


/////////////////////////////


int func() { return 1; }
int& func_lvalue_ref() { return func_lvalue_ref(); }
int&& func_rvalue_ref()  { return func_rvalue_ref(); }

// uncopyable & unmovable
class Class {
public:
    int member;
    Class() = default;
    Class(const Class&) = delete;
    Class(Class&&) = delete;
};

template <int I>
bool test_template_parameter_prvalue() {
    return IS_PRVALUE(I);
}


int main() {
    auto println = [](bool arg) {
        std::cout << (arg ? "true" : "false") << std::endl; 
    };

    println(IS_PRVALUE(1 + 2));
    println(IS_PRVALUE(Class{}));
    println(IS_PRVALUE(&main));
    println(IS_PRVALUE(main()));
    println(IS_PRVALUE(1));
    println(IS_PRVALUE(nullptr));
    println(test_template_parameter_prvalue<1>());
    // TODO lambda expression

    Class object;
    Class *pObject = &object;

    println(IS_LVALUE(object));
    println(IS_LVALUE(*pObject));
    println(IS_LVALUE(func_lvalue_ref()));
    // Note: 在字面值类型中，字符串字面值是特殊的，并非prvalue
    println(IS_LVALUE("shabi xiaomi"));
    println(IS_LVALUE(object.member));


    int eXpring = 1;

    println(IS_XVALUE(std::move(eXpring)));
    println(IS_XVALUE(func_rvalue_ref()));
    // Question: prvalue-to-xvalue应该是C++17及之后后才允许的
    //           然而这里使用-std=c++11仍然可以编译
    println(IS_XVALUE(std::move(Class{})));
    println(IS_XVALUE(Class{}.member));
    return 0;
}