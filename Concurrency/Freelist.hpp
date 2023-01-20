#pragma once
#include <bits/stdc++.h>
#include "Tagged_ptr.hpp"

// 实现lockfree的freelist


// 用于保证T的对齐肯定至少有指针大小
// 仅用于freelist内部
// 当T完成~T()时，T可以复用为freelist内部的node结点而无需数据域
template <typename T, typename Alloc_of_T = std::allocator<T>>
struct Wrapped_elem {
    alignas(std::ptrdiff_t) T data;
    Wrapped_elem() = default;
    template <typename ...Args>
    Wrapped_elem(Args &&...args): data(std::forward<Args>(args)...) {}

    // allocator for wrapped_elem
    using Alloc = typename std::allocator_traits<Alloc_of_T>
        ::template rebind_alloc<Wrapped_elem<T, Alloc_of_T>>;
}; 



// Alloc: allocator for typename T
// Wrapped_elem<T, Alloc>::Alloc: workaround for alignment
template <typename T, typename Alloc = std::allocator<T>>
class Freelist: Wrapped_elem<T, Alloc>::Alloc {
public:
    struct Node;
    using  Node_ptr = Tagged_ptr<Node>;

    struct Node { Node_ptr next; };

// wrapped T
private:
    using Wrapped_Alloc = typename Wrapped_elem<T, Alloc>::Alloc;

public:
    Freelist(size_t n = 0);

    ~Freelist();

public:
    template <bool Thread_safe = true>
    T* allocate();

    template <bool Thread_safe = true>
    void deallocate(T *elem);

private:
    T* allocate_impl_unsafe();

    T* allocate_impl_safe();

    void deallocate_impl_unsafe(T *elem);

    void deallocate_impl_safe(T *elem);

// extract: ? -> T
// pack: T -> ?
private:
    T* extract(Wrapped_elem<T, Alloc> *w) {
        return std::launder(reinterpret_cast<T*>(w));
    }
    T* extract(Node_ptr n) {
        return std::launder(reinterpret_cast<T*>(n.get_ptr()));
    }
    Node* pack(T *t) {
        return std::launder(reinterpret_cast<Node*>(t));
    }
private:
    std::atomic<Node_ptr> _pool;
};

template <typename T, typename Alloc_of_T>
Freelist<T, Alloc_of_T>::Freelist(size_t n): _pool(nullptr) {
    for(size_t i{}; i < n; ++i) {
        auto wrapped_ptr = Wrapped_Alloc::allocate(1);
        deallocate<false>(&wrapped_ptr->data);
    }
}

template <typename T, typename Alloc_of_T>
Freelist<T, Alloc_of_T>::~Freelist() {
    Node_ptr cur = _pool.load();
    while(cur) {
        Node_ptr tmp = cur->next;
        deallocate<false>(extract(cur));
        cur = tmp;
    }
}

template <typename T, typename Alloc_of_T>
template <bool Thread_safe>
T* Freelist<T, Alloc_of_T>::allocate() {
    if constexpr (Thread_safe) {
        return allocate_impl_safe();
    } else {
        return allocate_impl_unsafe();
    }
}

template <typename T, typename Alloc_of_T>
template <bool Thread_safe>
void Freelist<T, Alloc_of_T>::deallocate(T *elem) {
    if constexpr (Thread_safe) {
        deallocate_impl_safe(elem);
    } else {
        deallocate_impl_unsafe(elem);
    }
}

template <typename T, typename Alloc_of_T>
T* Freelist<T, Alloc_of_T>::allocate_impl_unsafe() {
    Node_ptr old_head = _pool.load(std::memory_order_relaxed);
    // no cache?
    if(!old_head) {
        return extract(Wrapped_Alloc::allocate(1));
    }
    // cached?
    Node_ptr new_head = old_head->next;
    _pool.store(new_head, std::memory_order_relaxed);
    return extract(old_head);
}

template <typename T, typename Alloc_of_T>
T* Freelist<T, Alloc_of_T>::allocate_impl_safe() {
    Node_ptr old_head = _pool.load(std::memory_order_acquire);
    while(1) {
        if(!old_head) return extract(Wrapped_Alloc::allocate(1));
        auto new_head = old_head->next;
        if(_pool.compare_exchange_weak(old_head, new_head)) {
            return extract(old_head);
        }
    }
}

template <typename T, typename Alloc_of_T>
void Freelist<T, Alloc_of_T>::deallocate_impl_unsafe(T *elem) {
    Node_ptr node = pack(elem);
    Node_ptr old_head = _pool.load(std::memory_order_relaxed);
    node->next = old_head;
    _pool.store(node, std::memory_order_relaxed);
}

template <typename T, typename Alloc_of_T>
void Freelist<T, Alloc_of_T>::deallocate_impl_safe(T *elem) {
    Node_ptr node = pack(elem);
    Node_ptr old_head = _pool.load(std::memory_order_acquire);
    while(1) {
        node->next = old_head;
        if(_pool.compare_exchange_weak(old_head, node,
                std::memory_order_release, std::memory_order_relaxed)) {
            return;
        }
    }
}
