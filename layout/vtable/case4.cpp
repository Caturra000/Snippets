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