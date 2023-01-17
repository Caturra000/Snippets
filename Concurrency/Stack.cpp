#include <bits/stdc++.h>

// 实现一个lockfree stack
// - 存储容器使用一个lockfree的freelist
// - 内存回收仅在析构时进行，用以简化实现

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

// 48bit pointer
// 只适用于x86_64四级分页的情况
// 使用高16bit作为tag避免ABA问题
// 如无特殊声明，tag是随机数
template <typename T>
struct Tagged_ptr {
    Tagged_ptr() = default;
    // Tagged_ptr(T *p) { std::memcpy(x, &p, 8); }
    Tagged_ptr(T *p, uint16_t t = rand()) { std::memcpy(x, &p, 6); x[3] = t;  }
    uint16_t get_tag() { return x[3]; }
    T* get_ptr() { return reinterpret_cast<T*>(to_val() & MASK);}
    T* operator->() { return get_ptr(); }
    T& operator*() { return *get_ptr(); }
    operator bool() { return !!get_ptr(); }
    uint64_t to_val() { return *reinterpret_cast<uint64_t*>(x); }
    // compared with tag
    bool operator==(Tagged_ptr p) { return to_val() == p.to_val(); }
    bool operator!=(Tagged_ptr p) { return !operator==(p);}
    constexpr static size_t MASK = (1ull<<48)-1;
    static uint16_t rand() {
        static thread_local size_t good = 1926'08'17;
        return good = (good * 998244353) + 12345;
    }
private:
    // low-48-bit: pointer
    // high-16-bit: tag
    uint16_t x[4];
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

template <typename T, typename Alloc_of_T = std::allocator<T>>
class Stack {
public:
    struct Node {
        T data;
        Tagged_ptr<Node> next;

        template <typename ...Args> Node(Args&&...args)
            : data(std::forward<Args>(args)...), next(nullptr){}
    };

    // Stack中实际分配使用的allocator
    using Node_Alloc = typename std::allocator_traits<Alloc_of_T>
                        ::template rebind_alloc<Node>;
    using Node_ptr = Tagged_ptr<Node>;

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
    Node_ptr new_node(node_ptr);
    auto old_head = _head.load(std::memory_order_relaxed);
    new_node->next = old_head;
    _head.store(new_node, std::memory_order_relaxed);
    return true;
}

template <typename T, typename Alloc_of_T>
template <typename ...Args>
bool Stack<T, Alloc_of_T>::do_push_impl_safe(Args &&...args) {
    auto node_ptr = _pool.allocate();
    new (node_ptr) Node(std::forward<Args>(args)...);
    if(!node_ptr) return false;
    Node_ptr new_node(node_ptr);
    auto old_head = _head.load(std::memory_order_acquire);
    for(;;) {
        new_node->next = old_head;
        if(_head.compare_exchange_weak(old_head, new_node,
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
    _pool.deallocate(old_head.get_ptr());
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
            _pool.deallocate(old_head.get_ptr());
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

struct HugeObject {
    std::vector<int> x;
    size_t y;
    HugeObject() = default;
    HugeObject(size_t i): x(100), y(i) {}
};

// 复用Queue的测试样例
// 相比不同的是，这次vector使用预分配，且写入到对应index中，这样可不使用mutex统计
void testStack() {
    Stack<HugeObject> q;
    
    constexpr size_t count = 1e6;
    constexpr size_t consumers = 5;
    constexpr size_t providers = 2;
    static_assert(count % consumers == 0);
    static_assert(count % providers == 0);
    std::vector<std::thread> consumer_threads;
    std::vector<std::thread> provider_threads;


    auto provider = [&q](size_t count, size_t start) {
        for(size_t i {start}; i < count + start; ++i) {
            q.push(HugeObject{i});
        }
    };
    std::vector<HugeObject> res(count);
    auto consumer = [&](size_t count) {
        HugeObject dummy;
        for(size_t i {}; i < count;) {
            if(!q.pop(dummy)) continue;
            res[dummy.y] = std::move(dummy);
            ++i;
        }
    };

    for(auto _ {consumers}; _--;) {
        consumer_threads.emplace_back(consumer, count / consumers);
    }
    for(auto i {providers}; i--;) {
        provider_threads.emplace_back(provider, count / providers, count / providers * i);
    }
    for(auto &&t : consumer_threads) t.join();
    for(auto &&t : provider_threads) t.join();
    // check sum
    size_t sum {};
    for(auto &&o : res) sum += o.y;
    for(size_t i {}; i < count; ++i) sum -=i;
    if(sum) {
        throw std::runtime_error("sum error");
    }
    // check [0, count)
    std::sort(res.begin(), res.end(), [](const auto &a, const auto &b) {
        return a.y < b.y;
    });
    std::for_each(res.begin(), res.end(), [v=0](auto &elem) mutable {
        if(elem.y != v) {
            throw std::runtime_error("elem error");
        }
        v++;
    });
    std::cout << "ok" << std::endl;
}

int main() {
    testStack();
}

struct {
    std::chrono::steady_clock::time_point clock_start, clock_end;
} global;

[[gnu::constructor]]
void global_start() {
    global.clock_start = std::chrono::steady_clock::now();
}

[[gnu::destructor]]
void global_end() {
    global.clock_end = std::chrono::steady_clock::now();
    using ToMilli = std::chrono::duration<double, std::milli>;
    auto elapsed = ToMilli{global.clock_end - global.clock_start}.count();
    std::cout << "elapsed: " << elapsed << "ms" << std::endl;
}
