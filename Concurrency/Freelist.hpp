#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <thread>
#include <cassert>
#include <iostream>
#include <ranges>
#include <memory>
#include <bit>
#include <cstddef>
#include <utility>
#include <new>
#include "Tagged_ptr.hpp"

// 实现lockfree的freelist

template <typename T, typename Alloc_of_T = std::allocator<T>>
struct Wrapped_elem {
    using Padding = std::byte[64];
    union { T data; Padding _; };
    Wrapped_elem(): data() {}
    template <typename ...Args>
    Wrapped_elem(Args &&...args): data(std::forward<Args>(args)...) {}
    ~Wrapped_elem() {data.~T();}

    // allocator for wrapped_elem
    using Internel_alloc = typename std::allocator_traits<Alloc_of_T>
        ::template rebind_alloc<Wrapped_elem<T, Alloc_of_T>>;

    struct Alloc: private Internel_alloc {
        T* allocate(size_t n, const void* h = 0) {
            [](...){}(h); // unused
            auto ptr = Internel_alloc::allocate(n);
            return &ptr->data;
        }

        void deallocate(T *p, size_t n) {
            Internel_alloc::deallocate(reinterpret_cast<Wrapped_elem*>(p), n);
        }
    };
}; 

template <typename T, typename Alloc = std::allocator<T>>
class Freelist: private Wrapped_elem<T, Alloc>::Alloc {
    struct Node { Tagged_ptr<Node> next; };

    // 分配T类型时实际使用的allocator
    using Custom_alloc = typename Wrapped_elem<T, Alloc>::Alloc;

public:
    Freelist();
    ~Freelist();

// 公开接口全部为线程安全实现
// 仿造std::allocator的基本接口
public:
    T* allocate();
    void deallocate(T* p);
    template <typename ...Args>
    void construct(T *p, Args &&...);
    void destroy(T *p);

private:
    std::atomic<Tagged_ptr<Node>> _pool;
};

template <typename T, typename Alloc>
Freelist<T, Alloc>::Freelist()
    : _pool{nullptr}
{}

template <typename T, typename Alloc>
Freelist<T, Alloc>::~Freelist() {
    Tagged_ptr<Node> cur = _pool.load();
    while(cur) {
        auto ptr = cur.get_ptr();
        if(ptr) cur = ptr->next;
        Custom_alloc::deallocate(reinterpret_cast<T*>(ptr), 1);
    }
}

template <typename T, typename Alloc>
T* Freelist<T, Alloc>::allocate() {
    Tagged_ptr<Node> old_head = _pool.load(std::memory_order_acquire);
    for(;;) {
        if(old_head == nullptr) {
            return Custom_alloc::allocate(1);
        }
        Tagged_ptr<Node> new_head = old_head->next;
        // Note
        new_head.set_tag(old_head.next_tag());
        if(_pool.compare_exchange_weak(old_head, new_head,
                std::memory_order_release, std::memory_order_relaxed)) {
            void *ptr = old_head.get_ptr();
            return reinterpret_cast<T*>(ptr);
        }
    }
}

template <typename T, typename Alloc>
void Freelist<T, Alloc>::deallocate(T *p) {
    Tagged_ptr<Node> old_head = _pool.load(std::memory_order_acquire);
    auto new_head_ptr = reinterpret_cast<Node*>(p);
    for(;;) {
        Tagged_ptr new_head {new_head_ptr, old_head.get_tag()};
        new_head->next = old_head;
        if(_pool.compare_exchange_weak(old_head, new_head,
                std::memory_order_release, std::memory_order_relaxed)) {
            return;
        }
    }
}

template <typename T, typename Alloc>
template <typename ...Args>
void Freelist<T, Alloc>::construct(T* p, Args &&...args) {
    new (p) T(std::forward<Args>(args)...);
}

template <typename T, typename Alloc>
void Freelist<T, Alloc>::destroy(T *p) {
    p->~T();
}
