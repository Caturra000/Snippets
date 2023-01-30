#include <cstdio>

// 两阶段命名查找：https://zhuanlan.zhihu.com/p/599328180

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
