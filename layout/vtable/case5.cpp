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