// $ g++ -c binding2.cpp -o binding2.o && readelf --symbols binding2.o | c++filt > dump

// FUNC    LOCAL  DEFAULT
static int local(void) { }

// FUNC    GLOBAL DEFAULT
int global(void) { }

// FUNC    WEAK   DEFAULT
int  __attribute__((weak)) weak(void) { }
