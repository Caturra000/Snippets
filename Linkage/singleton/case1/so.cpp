#include <iostream>
#include "singleton.h"

// so.cpp生成libso.so，提供给main使用

void f() {
    std::cout << &Singleton::getInstance() << std::endl;
}
