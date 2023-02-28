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
    // hide B::func_b
    void func_b() {}
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
  401112:	48 89 c7             	mov    %rax,%rdi
  401115:	e8 20 00 00 00       	callq  40113a <_ZN1A6func_aEv>
  40111a:	48 8d 45 f4          	lea    -0xc(%rbp),%rax
  40111e:	48 89 c7             	mov    %rax,%rdi
  401121:	e8 20 00 00 00       	callq  401146 <_ZN1C6func_bEv>
  this并不需要偏移，符合直觉
  401126:	48 8d 45 f4          	lea    -0xc(%rbp),%rax
  40112a:	48 89 c7             	mov    %rax,%rdi
  40112d:	e8 20 00 00 00       	callq  401152 <_ZN1C6func_cEv>
  401132:	b8 00 00 00 00       	mov    $0x0,%eax
  401137:	c9                   	leaveq 
  401138:	c3                   	retq   
  401139:	90                   	nop
*/
