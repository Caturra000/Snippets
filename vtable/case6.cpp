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