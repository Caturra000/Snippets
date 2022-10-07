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