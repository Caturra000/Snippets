#include <cstdio>

struct A {
    virtual void a() {}
};

struct B {
    virtual void b() {}
};

struct C {
    virtual void c() {}
};

struct D: A, B, C {
    virtual void d() {}
};

static auto println = [](auto *ptr) { printf("%p\n", ptr); };

int main() {
    D d;
    D *pd = &d;
    C *pc = &d;
    B *pb = &d;
    A *pa = &d;

    println(pd);
    println(pc);
    println(pb);
    println(pa);

    // safe
    auto spb = static_cast<B*>(&d);
    println(spb);

    // buggy code!
    auto rpb = reinterpret_cast<B*>(&d);
    println(rpb);
}

// output:
// 0x7fffffffdd70
// 0x7fffffffdd80
// 0x7fffffffdd78
// 0x7fffffffdd70
// 0x7fffffffdd78
// 0x7fffffffdd70
