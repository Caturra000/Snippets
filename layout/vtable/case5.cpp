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
  virtual void z() {}
  // 如果对v进行override，并不会有thunk
  void w() override {}
};



class Mother {
 public:
  virtual void MotherFoo() {}
};

class Father {
 public:
  virtual void FatherFoo() {}
};

class Child : public Mother, public Father {
 public:
  void FatherFoo() override {}
};

int main() {
    Mother *child = new Child();
    C c;
    return 0;
}

/*
godbolt输出

vtable for Child:
        .quad   0
        .quad   typeinfo for Child
        .quad   Mother::MotherFoo()
        .quad   Child::FatherFoo()
        .quad   -8
        .quad   typeinfo for Child
        .quad   non-virtual thunk to Child::FatherFoo()
vtable for Father:
        .quad   0
        .quad   typeinfo for Father
        .quad   Father::FatherFoo()
vtable for Mother:
        .quad   0
        .quad   typeinfo for Mother
        .quad   Mother::MotherFoo()
vtable for C:
        .quad   0
        .quad   typeinfo for C
        .quad   A::v()
        .quad   C::z()
        .quad   C::w()
        .quad   -16
        .quad   typeinfo for C
        .quad   non-virtual thunk to C::w()
vtable for B:
        .quad   0
        .quad   typeinfo for B
        .quad   B::w()
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::v()
typeinfo for Child:
        .quad   vtable for __cxxabiv1::__vmi_class_type_info+16
        .quad   typeinfo name for Child
        .long   0
        .long   2
        .quad   typeinfo for Mother
        .quad   2
        .quad   typeinfo for Father
        .quad   2050
typeinfo name for Child:
        .string "5Child"
typeinfo for Father:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for Father
typeinfo name for Father:
        .string "6Father"
typeinfo for Mother:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for Mother
typeinfo name for Mother:
        .string "6Mother"
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
