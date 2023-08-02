#include <cstdio>
// 这个符号引用会被2个动态库分别定义GLOBAL符号
// dso1.cpp -> libdso1.so提供1
// dso2.cpp -> libdso2.so提供2
//
// 除此以外还尝试定义一个WEAK符号
// dso3_weak.cpp -> libdso3_weak.so提供3
extern int multi_var;

int main() {
    // 如果是g++ main.cpp -o application -L. -ldso1 -ldso2
    // 那就得到1
    //
    // 反之，g++ main.cpp -o application -L. -ldso2 -ldso1
    // 那就得到2
    //
    // WEAK符号的定义也是会被链接器挑选的，并没有更低优先级的现象
    // g++ main.cpp -o application -L. -ldso3_weak -ldso1
    // 得到3
    printf("%d\n", multi_var);
    return 0;
}