// $ g++ -c binding2.cpp -o binding2.o && readelf --symbols binding2.o | c++filt > dump

// FUNC    LOCAL  DEFAULT
static int local(void) { return 0; }
static int local2(void);

// FUNC    GLOBAL DEFAULT
int global(void) { return 0; }
int global2(void);

// FUNC    WEAK   DEFAULT
int  __attribute__((weak)) weak(void) { return 0; }
int  __attribute__((weak)) weak2(void);

inline int none(void) { return 0; }
inline int none2(void);

extern int ext(void);
