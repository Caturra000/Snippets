#include <cstddef>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>


// non-templated
// deprecated since C++20
struct Alloc_base {
    template <typename T, typename ...Args>
    void construct(T *p, Args &&...args) {
        new (p) T(std::forward<Args>(args)...);
    }

    template <typename T>
    void destroy(T *p) {
        p->~T();
    }
};

template <typename T, size_t Capacity>
class Fixed_allocator
    : public Alloc_base {
public:
    T* allocate(size_t n) {
        if(_used + n > Capacity) {
            throw std::bad_alloc();
        }
        auto p = std::launder(reinterpret_cast<T*>(_buffer[_used]));
        _used += n;
        return p;
    }

    void deallocate(T *, size_t n) {
        _used -= n;
    }

private:
    size_t _used {0};
    alignas(T) std::byte _buffer[Capacity][sizeof(T)]; 
};

int main() {
    constexpr size_t capacity = 100;
    Fixed_allocator<std::string, capacity> alloc;
    using String_ptr = std::string*;
    String_ptr ptrs[capacity]; // backup
    for(size_t i {0}; i < capacity; ++i) {
        auto p = alloc.allocate(1);
        alloc.construct(p, "xyz");
        ptrs[i] = p;
    }
    for(auto p : ptrs) {
        std::cout << *p << std::endl;
        alloc.destroy(p);
        alloc.deallocate(p, 1);
    }
    return 0;
}