#include <iostream>
#include "singleton.h"

// 来自libso
extern void f();

int main() {
    // header singleton
    std::cout << &Singleton::getInstance() << std::endl;
    // library singleton
    f();
}
