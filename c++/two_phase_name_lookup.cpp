#include <cstdio>

// 两阶段命名查找：https://zhuanlan.zhihu.com/p/599328180
//
// 更多：
// https://en.cppreference.com/w/cpp/language/lookup
// https://en.cppreference.com/w/cpp/language/qualified_lookup
// https://en.cppreference.com/w/cpp/language/unqualified_lookup

void foo(double) { puts("double"); }

template <typename T>
void bar() {
    foo(1);     // #1
    foo(T{});   // #2
}

void foo(int)  { puts("int"); }

int main() {
    // 此时foo均选择了foo(double)
    bar<int>();
}
