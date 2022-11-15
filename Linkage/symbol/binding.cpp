// 如果生成.o，那么全部binding方式都是LOCAL（或者不存在这个符号）
// $ g++ -c binding.cpp --std=c++17 -o binding.o && readelf --symbols binding.o | c++filt > dump

extern int a;

static int b0;
static int b1 = 0;
static int b2 = 1;

// -stdc++=17
inline int c0;
inline int c1 = 0;
inline int c2 = 2;

extern int funca(double);

static int funcb0(double);
static int funcb1(double) {return 0;}

inline int funcc0(double);
inline int funcc1(double) {return 1;}