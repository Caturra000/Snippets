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

}
