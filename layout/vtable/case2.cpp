#include <bits/stdc++.h>

struct A {
    virtual void func() {};
};

int main() {
    A a;
    return 0;
}

/*
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::func()
typeinfo for A:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for A
typeinfo name for A:
        .string "1A"
*/
