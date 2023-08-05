#include <stdio.h>
int x;

// 即使是较新版本的gcc（11.3.0）
// 也可以通过gcc -fcommon case2_main.c case2_obj.c完成编译链接
// 即让未初始化变量放到COMMON里面
// 但是linker可以发现（警告）另一个object文件的double x存在对齐问题
int main() {
    printf("%d\n", x);
    return 0;
}
