#include <iostream>

void f2();

void f1() {
    f2();
    std::cout << "f1()" << std::endl;
}