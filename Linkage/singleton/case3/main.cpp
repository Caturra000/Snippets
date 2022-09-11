#include <iostream>
#include "singleton.h"

// 来自libso
extern void f();

int main() {
    // header singleton
    std::cout << &getInstance() << std::endl;
    // library singleton
    f();
    // 此时输出的地址并不相同
}
