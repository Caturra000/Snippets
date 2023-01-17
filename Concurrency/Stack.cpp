#include <bits/stdc++.h>

// 实现一个lockfree stack
// - 存储容器使用一个lockfree的freelist
// - 内存回收仅在析构时进行，用以简化实现

// !! WORK IN PROGESS !!
// !! WORK IN PROGESS !!
// !! WORK IN PROGESS !!

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
    struct Node { Node *next; };

    // TODO 可能改为Tagged_node_ptr，用以避免ABA
    using Node_ptr = Node*;
    // TODO 可能是一个实际的数值下标
    // using Index = T*;

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
        return std::launder(reinterpret_cast<T*>(n));
    }
    Node_ptr pack(T *t) {
        return std::launder(reinterpret_cast<Node_ptr>(t));
    }
private:
    std::atomic<Node_ptr> _pool;
};

template <typename T, typename Alloc_of_T = std::allocator<T>>
class Stack {
public:
    struct Node {
        T data;
        Node *next;

        template <typename ...Args> Node(Args&&...args)
            : data(std::forward<Args>(args)...), next(nullptr){}
    };

    // Stack中实际分配使用的allocator
    using Node_Alloc = typename std::allocator_traits<Alloc_of_T>
                        ::template rebind_alloc<Node>;
    using Node_ptr = Node*;

public:
    Stack();
    ~Stack();

public:
    template <typename ...Args>
    bool push(Args &&...);

    bool pop(T &);

    bool empty();
private:

    template <bool Thread_safe, typename ...Args>
    bool do_push(Args &&...);
    template <typename ...Args>
    bool do_push_impl_unsafe(Args &&...);
    template <typename ...Args>
    bool do_push_impl_safe(Args &&...);

    template <bool Thread_safe>
    bool do_pop(T &out);
    bool do_pop_impl_unsafe(T &out);
    bool do_pop_impl_safe(T &out);

private:
    // cacheline == 64 bytes
    // 将跨越2个cacheline

    alignas(64)
    std::atomic<Node_ptr> _head;

    alignas(64)
    Freelist<Node, Node_Alloc> _pool;
};

template <typename T, typename Alloc_of_T>
Stack<T, Alloc_of_T>::Stack()
    : _head(nullptr)
{}

template <typename T, typename Alloc_of_T>
Stack<T, Alloc_of_T>::~Stack() {
    for(T dummy; do_pop<false>(dummy););
}

template <typename T, typename Alloc_of_T>
template <typename ...Args>
bool Stack<T, Alloc_of_T>::push(Args &&...args) {
    return do_push<true>(std::forward<Args>(args)...);
}

template <typename T, typename Alloc_of_T>
bool Stack<T, Alloc_of_T>::pop(T &out) {
    return do_pop<true>(out);
}

template <typename T, typename Alloc_of_T>
bool Stack<T, Alloc_of_T>::empty() {
    return !_head.load(std::memory_order_relaxed);
}

template <typename T, typename Alloc_of_T>
template <bool Thread_safe, typename ...Args>
bool Stack<T, Alloc_of_T>::do_push(Args &&...args) {
    if constexpr (Thread_safe) {
        return do_push_impl_safe(std::forward<Args>(args)...);
    } else {
        return do_push_impl_unsafe(std::forward<Args>(args)...);
    }
}

template <typename T, typename Alloc_of_T>
template <typename ...Args>
bool Stack<T, Alloc_of_T>::do_push_impl_unsafe(Args &&...args) {
    auto node_ptr = _pool.allocate();
    // TODO 这里需要异常处理，避免node_ptr泄露
    // 要不还是别仿造allocator了？
    new (node_ptr) Node(std::forward<Args>(args)...);
    if(!node_ptr) return false;
    auto old_head = _head.load(std::memory_order_relaxed);
    node_ptr->next = old_head;
    _head.store(node_ptr, std::memory_order_relaxed);
    return true;
}

template <typename T, typename Alloc_of_T>
template <typename ...Args>
bool Stack<T, Alloc_of_T>::do_push_impl_safe(Args &&...args) {
    auto node_ptr = _pool.allocate();
    new (node_ptr) Node(std::forward<Args>(args)...);
    if(!node_ptr) return false;
    auto old_head = _head.load(std::memory_order_acquire);
    for(;;) {
        node_ptr->next = old_head;
        if(_head.compare_exchange_weak(old_head, node_ptr,
                std::memory_order_release, std::memory_order_relaxed)) {
            break;
        }
    }
    return true;
}

template <typename T, typename Alloc_of_T>
template <bool Thread_safe>
bool Stack<T, Alloc_of_T>::do_pop(T &out) {
    if constexpr (Thread_safe) {
        return do_pop_impl_safe(out);
    } else {
        return do_pop_impl_unsafe(out);
    }
}

template <typename T, typename Alloc_of_T>
bool Stack<T, Alloc_of_T>::do_pop_impl_unsafe(T &out) {
    auto old_head = _head.load(std::memory_order_relaxed);
    if(!old_head) return false;
    auto new_head = old_head->next;
    _head.store(new_head, std::memory_order_relaxed);
    out = std::move(old_head->data);
    old_head->~Node();
    _pool.deallocate(old_head);
    return true;
}

template <typename T, typename Alloc_of_T>
bool Stack<T, Alloc_of_T>::do_pop_impl_safe(T &out) {
    auto old_head = _head.load(std::memory_order_acquire);
    for(;;) {
        // Note: 每次都有可能为nullptr
        if(!old_head) return false;
        auto new_head = old_head->next;
        if(_head.compare_exchange_weak(old_head, new_head,
                std::memory_order_release, std::memory_order_relaxed)) {
            out = std::move(old_head->data);
            old_head->~Node();
            _pool.deallocate(old_head);
            break;
        }
    }
    return true;
}

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

int main() {
    // std::allocator<std::string> alloc;
    // Freelist<std::string, decltype(alloc)> list;
    // for(int t = 100; t--;) {
    //     auto ps1 = list.allocate<false>();
    //     alloc.construct(ps1, "abc");
    //     std::cout << *ps1 << std::endl;
    //     alloc.destroy(ps1);
    //     list.deallocate<false>(ps1);
    // }

    Stack<std::string> stk;
    stk.push("RGB");
    std::string out;
    stk.pop(out);
    std::cout << out << std::endl;

    // TODO test
    return 0;
}