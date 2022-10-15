#include <bits/stdc++.h>

class A {
public:
  int a;
  virtual void v() {}
};

class B {
public:
  int b;
  virtual void w() {}
};

class C : public A, public B {
public:
  int c;
};

int main() {
    C c;
    return 0;
}

/*
vtable for C:
        .quad   0
        .quad   typeinfo for C
        .quad   A::v()
        .quad   -16
        .quad   typeinfo for C
        .quad   B::w()
vtable for B:
        .quad   0
        .quad   typeinfo for B
        .quad   B::w()
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::v()
typeinfo for C:
        .quad   vtable for __cxxabiv1::__vmi_class_type_info+16
        .quad   typeinfo name for C
        .long   0
        .long   2
        .quad   typeinfo for A
        .quad   2
        .quad   typeinfo for B
        .quad   4098
typeinfo name for C:
        .string "1C"
typeinfo for B:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for B
typeinfo name for B:
        .string "1B"
typeinfo for A:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for A
typeinfo name for A:
        .string "1A"
*/
