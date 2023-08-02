#include <cstdio>
// 这个值可能由global.cpp -> libglobal.so提供
// 也可能由weak.cpp -> libweak.so提供
extern int var;

int main() {
    // 如果var是GLOBAL，那没啥好说，当然没问题
    // 如果var是WEAK，仍然会找到定义，但这个似乎是看linker具体实现
    // - 仅仅是生成目标文件.o（-c）的话，var符号是GLOBAL UNDEFINED
    // - 如果是生成可执行文件（-L -l）的话，var符号是WEAK DEFINED
    printf("%d\n", var);
    return 0;
}