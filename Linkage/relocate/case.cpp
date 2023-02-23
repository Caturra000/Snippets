static int static_var;
extern int extern_var;
int global_var = 6;

static void static_func() {}
extern void extern_func();
void global_func() {}

void test_relocate() {
    static_var = 1;
    extern_var = 2;
    global_var = 3;

    static_func();
    extern_func();
    global_func();
}
