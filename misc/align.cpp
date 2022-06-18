#include <bits/stdc++.h>

struct Case1 {
    int a;
    char b;
};

struct Case2 {
    int a;
    char b;
    char c;
};

struct Case3 {
    long a;
    char b;
};

struct Case4 {
    uint16_t a;
    uint32_t b;
    uint8_t c;
};

// 可直接类比int32 int64 int128
// 看下面Case5和Case6对比
// N为位数
template <size_t N>
struct Large { char x[N/8]; };

struct Case5 {
    uint16_t a;
    Large<128> b;
    uint8_t c;
};

struct Case6 {
    uint16_t a;
    __int128_t b;
    uint8_t c;
};

// struct Case7 {
//     char a;
//     Large<32> b;
//     char c;
// };

// struct Case8 {
//     char a;
//     uint32_t b;
//     char c;
// };

struct Case7 {
    char a;
    char b;
    char c;
    char d;
    char e;
};

struct Case8 {
    char a;
    uint32_t b;
};

int main() {

    // 8
    sizeof(Case1);
    // 8
    sizeof(Case2);
    // 16
    sizeof(Case3);

    // 12
    sizeof(Case4);
    // 20
    sizeof(Case5);
    // 32
    sizeof(Case6);

    // 期望从Case6和Case7中构造一个非对齐访问的例子
    // 5
    sizeof(Case7);
    // 8
    sizeof(Case8);
    Case7 case7;
    Case8 case8;
    // 可以尝试改成case7.a对比指令（此时对齐）
    // - 丢到godbolt可以看出会多了一条指令，<del>也得出非原子访问的结论</del>
    // - update. 似乎并不能直接说明，是否原子从指令上来说仍然没有说服力
    //           首先多的指令只是%rax地址偏移，为了可以对(%rax)进行movq
    //           其次开了O3还是能做到直接mov的
    //           还是看SDM手册描述吧，牙膏厂说啥就是啥
    *((uint32_t*)(&case7.b)) = 1234;
    case8.b = 1234;

}
