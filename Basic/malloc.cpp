#include <sys/mman.h>
#include <cstdio>

void* myMalloc(size_t size) {
    void *addr = ::mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if(addr == MAP_FAILED) return nullptr;
    return addr;
}

// TODO myFree

int main() {
    auto addr = myMalloc(123);
    int *testInt = (int*)addr;
    *testInt = 123;
    ::printf("%d\n", *testInt);
    return 0;
}