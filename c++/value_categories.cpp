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

#define QUERY(e) (IS_LVALUE(e) ? "lvalue" \
                : IS_XVALUE(e) ? "xvalue" \
                : "prvalue")

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

#if __cplusplus >= 201402L

decltype(auto) cpp14_test1(int i) {
    // prvalue
    return i;
}

decltype(auto) cpp14_test2(int i) {
    // lvalue
    return (i);
}

decltype(auto) cpp14_test3() {
    // prvalue
    // 类似于"return i"
    return Class{}.member;
}
decltype(auto) cpp14_test4() {
    // xvalue
    // 不同于"return (i)"
    return (Class{}.member);
}

void test_decltype_auto() {
    std::cout << QUERY(cpp14_test1(0)) << std::endl
              << QUERY(cpp14_test2(0)) << std::endl
              << QUERY(cpp14_test3()) << std::endl
              << QUERY(cpp14_test4()) << std::endl;
}
#else
void test_decltype_auto() {}
#endif

int main() {
    auto println = [](bool arg) {
        static char once {(std::cout << std::boolalpha, 'x')};
        std::cout << arg << std::endl;
    };

    int variable = 0;

    println(IS_PRVALUE(1 + 2));
    println(IS_PRVALUE(variable + 1));
    println(IS_PRVALUE(Class{}));
    println(IS_PRVALUE(&main));
    println(IS_PRVALUE(main()));
    println(IS_PRVALUE(1));
    println(IS_PRVALUE(nullptr));
    println(IS_PRVALUE(variable++));
    println(IS_PRVALUE((variable>=0 ? variable : 0)));
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
    println(IS_LVALUE(++variable));
    println(IS_LVALUE((variable>=0 ? variable : variable)));

    int eXpring = 1;

    println(IS_XVALUE(std::move(eXpring)));
    println(IS_XVALUE(func_rvalue_ref()));
    // Question: prvalue-to-xvalue应该是C++17及之后后才允许的
    //           然而这里使用-std=c++11仍然可以编译
    println(IS_XVALUE(std::move(Class{})));
    println(IS_XVALUE(Class{}.member));

    test_decltype_auto();
    return 0;
}