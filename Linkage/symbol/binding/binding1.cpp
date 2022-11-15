// $ g++ -c binding1.cpp --std=c++17 -o binding1.o && readelf --symbols binding1.o | c++filt > dump

extern int a;

// OBJECT  LOCAL  DEFAULT
static int b0;
// OBJECT  LOCAL  DEFAULT
static int b1 = 0;
// OBJECT  LOCAL  DEFAULT
static int b2 = 1;

// -stdc++=17
inline int c0;
inline int c1 = 0;
inline int c2 = 2;

// OBJECT  GLOBAL DEFAULT
int d0;
// OBJECT  GLOBAL DEFAULT
int d1 = 0;
// OBJECT  GLOBAL DEFAULT
int d2 = 3;
