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

    std::optional<T> pop();

    bool empty();

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
    while(pop());
}

template <typename T, typename Alloc_of_T>
template <typename ...Args>
bool Stack<T, Alloc_of_T>::push(Args &&...args) {
    Node *ptr = _pool.allocate();
    // 为了简化，这里并不处理异常安全
    if(!ptr) return false;
    _pool.construct(ptr, std::forward<Args>(args)...);
    Tagged_ptr<Node> old_head = _head.load(std::memory_order_acquire);
    for(;;) {
        Node_ptr new_head {ptr, old_head.get_tag()};
        new_head->next = old_head;
        if(_head.compare_exchange_weak(old_head, new_head)) {
            return true;
        }
    }
}

template <typename T, typename Alloc_of_T>
std::optional<T> Stack<T, Alloc_of_T>::pop() {
    Tagged_ptr<Node> old_head = _head.load(std::memory_order_acquire);
    for(;;) {
        if(!old_head) return std::nullopt;
        Tagged_ptr<Node> new_head = old_head->next;
        new_head.set_tag(old_head.next_tag());
        if(_head.compare_exchange_weak(old_head, new_head)) {
            auto opt = std::make_optional<T>(old_head->data);
            auto ptr = old_head.get_ptr();
            _pool.destroy(ptr);
            _pool.deallocate(ptr);
            return opt;
        }
    }
}

template <typename T, typename Alloc_of_T>
bool Stack<T, Alloc_of_T>::empty() {
    return !_head.load(std::memory_order_relaxed);
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
        for(size_t i {}; i < count;) {
            auto opt = q.pop();
            if(!opt) continue;
            ++i;
            while(!receiver.push(std::move(*opt)));
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
    std::vector<HugeObject> res;

    while(!receiver.empty()) {
        std::optional<HugeObject> opt = receiver.pop();
        res.emplace_back(std::move(*opt));
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
