#include <bits/stdc++.h>
struct A
{
    int ax;
    virtual void f0() {}
    virtual void bar() {}
};

struct B : virtual public A           /****************************/
{                                     /*                          */
    int bx;                           /*             A            */
    void f0() override {}             /*           v/ \v          */
};                                    /*           /   \          */
                                      /*          B     C         */
struct C : virtual public A           /*           \   /          */
{                                     /*            \ /           */
    int cx;                           /*             D            */
    void f0() override {}             /*                          */
};                                    /****************************/

struct D : public B, public C
{
    int dx;
    void f0() override {}
};


int main() {
    D d;
    return 0;
}

/*
vtable for D:
        .quad   32
        .quad   0
        .quad   typeinfo for D
        .quad   D::f0()
        .quad   16
        .quad   -16
        .quad   typeinfo for D
        .quad   non-virtual thunk to D::f0()
        .quad   0
        .quad   -32
        .quad   -32
        .quad   typeinfo for D
        .quad   virtual thunk to D::f0()
        .quad   A::bar()
VTT for D:
        .quad   vtable for D+24
        .quad   construction vtable for B-in-D+24
        .quad   construction vtable for B-in-D+64
        .quad   construction vtable for C-in-D+24
        .quad   construction vtable for C-in-D+64
        .quad   vtable for D+96
        .quad   vtable for D+56
construction vtable for B-in-D:
        .quad   32
        .quad   0
        .quad   typeinfo for B
        .quad   B::f0()
        .quad   0
        .quad   -32
        .quad   -32
        .quad   typeinfo for B
        .quad   virtual thunk to B::f0()
        .quad   A::bar()
construction vtable for C-in-D:
        .quad   16
        .quad   0
        .quad   typeinfo for C
        .quad   C::f0()
        .quad   0
        .quad   -16
        .quad   -16
        .quad   typeinfo for C
        .quad   virtual thunk to C::f0()
        .quad   A::bar()
vtable for A:
        .quad   0
        .quad   typeinfo for A
        .quad   A::f0()
        .quad   A::bar()
typeinfo for D:
        .quad   vtable for __cxxabiv1::__vmi_class_type_info+16
        .quad   typeinfo name for D
        .long   2
        .long   2
        .quad   typeinfo for B
        .quad   2
        .quad   typeinfo for C
        .quad   4098
typeinfo name for D:
        .string "1D"
typeinfo for C:
        .quad   vtable for __cxxabiv1::__vmi_class_type_info+16
        .quad   typeinfo name for C
        .long   0
        .long   1
        .quad   typeinfo for A
        .quad   -6141
typeinfo name for C:
        .string "1C"
typeinfo for B:
        .quad   vtable for __cxxabiv1::__vmi_class_type_info+16
        .quad   typeinfo name for B
        .long   0
        .long   1
        .quad   typeinfo for A
        .quad   -6141
typeinfo name for B:
        .string "1B"
typeinfo for A:
        .quad   vtable for __cxxabiv1::__class_type_info+16
        .quad   typeinfo name for A
typeinfo name for A:
        .string "1A"
*/
