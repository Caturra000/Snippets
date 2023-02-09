#include <iostream>

struct Empty {};

struct EBO_class : Empty { int tmp; };

struct Unique_class {
    Empty e;
    int tmp;
};

struct Class {
    [[no_unique_address]] Empty e;
    int tmp;
};

int main() {
    // 1 4 8 4
    std::cout << sizeof(Empty) << std::endl
              << sizeof(EBO_class) << std::endl
              << sizeof(Unique_class) << std::endl
              << sizeof(Class) << std::endl;
}
