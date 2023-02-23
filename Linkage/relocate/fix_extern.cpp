int extern_var = 123;
void extern_func() {}

extern void test_relocate();

int main() {
    test_relocate();
}
