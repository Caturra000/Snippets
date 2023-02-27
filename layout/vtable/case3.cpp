#include <bits/stdc++.h>

struct A {
    virtual void func0() {}
    virtual A* func1() { return this; }
};

struct B: public A {
    void func0() override {}
    virtual void func2() {}
};

struct C: public A {
    // reorder declarations
    virtual void func2() {}
    void func0() override {}
};

struct D: public A {
    // covariant return type override (A* -> D*)
    // https://en.cppreference.com/w/cpp/language/virtual#:~:text=Covariant%20return%20types
    D* func1() override { return nullptr; }
    virtual void func2() {}
};

int main() {
    A a;
    B b;
    C c;
    D d;
    return 0;
}

/*
vtable for D:
        .quad   0
        .quad   typeinfo for D
        .quad   A::func0()
        .quad   D::func1()
        .quad   D::func2()
vtable for C:
        .quad   0
        .quad   typeinfo for C
        .quad   C::func0()
        .quad   A::func1()
        .quad   C::func2()
vtable for B:
        .quad   0
        .quad   typeinfo for B
        .quad   B::func0()
        .quad   A::func1()
        .quad   B::func2()
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::func0()
        .quad   A::func1()
typeinfo for D:
        .quad   vtable for __cxxabiv1::__si_class_type_info+16
        .quad   typeinfo name for D
        .quad   typeinfo for A
typeinfo name for D:
        .string "1D"
typeinfo for C:
        .quad   vtable for __cxxabiv1::__si_class_type_info+16
        .quad   typeinfo name for C
        .quad   typeinfo for A
typeinfo name for C:
        .string "1C"
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
