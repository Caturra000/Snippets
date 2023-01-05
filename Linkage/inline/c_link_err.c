#include <stdio.h>

inline int max(int a, int b) {
    return a > b ? a : b;
}

int main() {
    // C在处理inline的方式与C++不一样
    // 不含extern标记的inline实现会直接抛弃符号
    // 因此`$ gcc c_link_err.c`时ld会报错：undefined reference to `max'
    // 更多讨论：https://www.zhihu.com/question/574760776
    int t = max(1, 2);
    printf("%d\n", t);
    return 0;
}
