#include <sys/mman.h>
#include <iostream>

// prefault也会影响匿名页，使用/usr/bin/time ./prefault可分析得知
// 之所以特意写这个测试，是因为man手册只会说谜语
/*
MAP_POPULATE (since Linux 2.5.46)
       Populate  (prefault)  page tables for a mapping.  For a file mapping, this
       causes read-ahead on the file.  This will help to reduce blocking on  page
       faults later.  The mmap() call doesn't fail if the mapping cannot be popu‐
       lated  (for example, due to limitations on the number of mapped huge pages
       when using MAP_HUGETLB).  Support for  MAP_POPULATE  in  conjunction  with
       private mappings was added in Linux 2.6.23.
*/
constexpr bool CONFIG_PREFAULT = true;
constexpr bool CONFIG_TOUCH = false;

int main() {
    auto explicit_malloc = [](size_t size) -> void* {
        auto flags = MAP_PRIVATE | MAP_ANONYMOUS | (CONFIG_PREFAULT ? MAP_POPULATE : 0);
        auto addr = ::mmap(0, size, PROT_READ | PROT_WRITE, flags, 0, 0);
        if(addr == MAP_FAILED) return nullptr;
        return addr;
    };

    constexpr size_t count = 1e6;
    size_t warnings = 0;
    for(auto n = count; n--;) {
        auto addr = static_cast<char*>(explicit_malloc(1));
        if(!addr) n++, warnings++;
        else if constexpr (CONFIG_TOUCH) *addr = 'z';
    }

    if(warnings) std::cout << warnings << std::endl;
}
