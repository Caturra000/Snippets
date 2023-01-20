#include <bits/stdc++.h>
#include "Tagged_ptr.hpp"
#include "Freelist.hpp"
#include "util/timer.hpp"

// 实现一个lockfree stack
// - 存储容器使用一个lockfree的freelist
// - 内存回收仅在析构时进行，用以简化实现
// - 只关注lockfree本身，基本的类设计并不完善

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

struct HugeObject {
    std::vector<int> x;
    size_t y;
    HugeObject() = default;
    HugeObject(size_t i): x(100), y(i) {}
};

// 使用类似Queue.cpp的测试样例
// 但是最后验证用的容器也是lockfree Stack
void testStack() {
    Stack<HugeObject> q;
    
    constexpr size_t count = 5e6;
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
    Stack<HugeObject> receiver;
    auto consumer = [&](size_t count) {
        HugeObject dummy;
        for(size_t i {}; i < count;) {
            if(!q.pop(dummy)) continue;
            ++i;
            while(!receiver.push(dummy));
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
    HugeObject dummy;
    std::vector<HugeObject> res;

    while(!receiver.empty()) {
        while(!receiver.pop(dummy));
        res.emplace_back(std::move(dummy));
    }
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
