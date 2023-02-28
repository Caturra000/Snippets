struct A {
    int a;
    void func_a() {}
};

struct B {
    int b;
    void func_b() {}
};

struct C: A, B {
    int c;
    void func_c() {}
};

int main() {
    C c;
    c.func_a();
    c.func_b();
    c.func_c();
}

/*
0000000000401106 <main>:
  401106:	55                   	push   %rbp
  401107:	48 89 e5             	mov    %rsp,%rbp
  40110a:	48 83 ec 10          	sub    $0x10,%rsp
  40110e:	48 8d 45 f4          	lea    -0xc(%rbp),%rax
  this等于%rbp-$0xc
  401112:	48 89 c7             	mov    %rax,%rdi
  401115:	e8 24 00 00 00       	callq  40113e <_ZN1A6func_aEv>
  40111a:	48 8d 45 f4          	lea    -0xc(%rbp),%rax
  40111e:	48 83 c0 04          	add    $0x4,%rax
  在这里this+4然后调用func_b
  401122:	48 89 c7             	mov    %rax,%rdi
  401125:	e8 20 00 00 00       	callq  40114a <_ZN1B6func_bEv>
  作为成员函数接受的是B*类型，因此需要调整
  40112a:	48 8d 45 f4          	lea    -0xc(%rbp),%rax
  40112e:	48 89 c7             	mov    %rax,%rdi
  401131:	e8 20 00 00 00       	callq  401156 <_ZN1C6func_cEv>
  401136:	b8 00 00 00 00       	mov    $0x0,%eax
  40113b:	c9                   	leaveq 
  40113c:	c3                   	retq   
  40113d:	90                   	nop
*/
