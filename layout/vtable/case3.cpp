#include <bits/stdc++.h>

struct A {
    virtual void func0() {}
    virtual void func1() {}
};

struct B: public A {
    void func0() override {}
};

int main() {
    A a;
    B b;
    return 0;
}

/*
vtable for B:
        .quad   0
        .quad   typeinfo for B
        .quad   B::func0()
        .quad   A::func1()
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::func0()
        .quad   A::func1()
typeinfo for B:
        .quad   vtable for __cxxabiv1::__si_class_type_info+16
        .quad   typeinfo name for B
        .quad   typeinfo for A
typeinfo name for B:
        .string "1B"
typeinfo for A:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for A
typeinfo name for A:
        .string "1A"
*/
