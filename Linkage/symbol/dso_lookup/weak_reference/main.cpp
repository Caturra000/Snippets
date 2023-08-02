#include <cstdio>

// 情况1：尝试从weak.cpp -> libweak.so获取符号的定义
// 情况2：也可能根本不找符号（nothing.cpp -> libnothing.so）
// 情况3：动态库中提供同名GLOBAL符号（global.cpp -> libglobal.so）
[[gnu::weak]]
extern int weak_var;

int main() {
    // 如果可执行文件是链接了libweak.so：
    // - 会segment fault
    // - 似乎WEAK引用并不会在DSO查找WEAK定义
    //
    // 如果是链接了libnothing.so：
    // - 注意这个时候weak_var仍然没有定义
    // - 也并不会引发任何链接报错
    // - 也是segment fault
    //
    // 即使是链接了libglobal.so
    // - 仍然是挂掉
    printf("%d\n", weak_var);
    return 0;
}