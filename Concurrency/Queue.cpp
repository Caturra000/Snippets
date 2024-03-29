#include <bits/stdc++.h>

// 并发上的Queue，有锁粗粒度版本（V1）
// 不考虑拷贝和移动
// 不考虑异常安全
// 不考虑通用容器
template <typename T>
class Queue {
public:
    Queue() = default;
    ~Queue() = default;

    // 目前不允许拷贝和移动
    Queue(const Queue &) = delete;
    Queue& operator=(const Queue &) = delete;

// 较为简化的用户接口
public:
    // bool     empty() const;
    // size_t   size() const;
    // T&       front();
    // const T& front() const;
    // T&       back();
    // const T& back() const;
    // void     push(const T &);
    // void     push(T &&);
    // template <typename ...Args>
    // void     emplace(Args &&...);
    // void     pop();

    void push(T);
    bool try_pop(T &);
    auto try_pop() -> std::shared_ptr<T>;
    void wait_and_pop(T &);
    auto wait_and_pop() -> std::shared_ptr<T>;
    bool empty() const;

private:
    std::deque<T> _container;
    mutable std::mutex _mutex;
    std::condition_variable _condition;
};

template <typename T>
void Queue<T>::push(T value) {
    {
        std::lock_guard<std::mutex> _ {_mutex};
        _container.push_back(std::move(value));
    }
    _condition.notify_one();
}

template <typename T>
bool Queue<T>::try_pop(T &out) {
    std::lock_guard<std::mutex> _ {_mutex};
    if(_container.empty()) return false;
    out = std::move(_container.front());
    _container.pop_front();
    return true;
}

template <typename T>
auto Queue<T>::try_pop() -> std::shared_ptr<T> {
    std::lock_guard<std::mutex> _ {_mutex};
    if(_container.empty()) return nullptr;
    auto elem = std::make_shared<T>(std::move(_container.front()));
    _container.pop_front();
    return elem;
}

template <typename T>
void Queue<T>::wait_and_pop(T &out) {
    std::unique_lock<std::mutex> guard {_mutex};
    _condition.wait(guard, [this] {
        return !_container.empty();
    });
    out = std::move(_container.front());
    _container.pop_front();
}

template <typename T>
auto Queue<T>::wait_and_pop() -> std::shared_ptr<T> {
    std::unique_lock<std::mutex> guard {_mutex};
    _condition.wait(guard, [this] {
        return !_container.empty();
    });
    auto elem = std::make_shared<T>(std::move(_container.front()));
    _container.pop_front();
    return elem;
}

template <typename T>
bool Queue<T>::empty() const {
    std::lock_guard<std::mutex> _ {_mutex};
    return _container.empty();
}














template <typename Q>
struct Queue_v2_helper {
    auto get_tail_copy();
    auto pop_head();
};

// 更细粒度
template <typename T>
class Queue_v2: private Queue_v2_helper<Queue_v2<T>> {
public:
    Queue_v2() = default;
    ~Queue_v2() = default;

    Queue_v2(const Queue_v2 &) = delete;
    Queue_v2& operator=(const Queue_v2 &) = delete;

public:
    void push(T);
    bool try_pop(T &);
    auto try_pop() -> std::shared_ptr<T>;
    // 有空再写
    // void wait_and_pop(T &);
    // auto wait_and_pop() -> std::shared_ptr<T>;
    // bool empty() const;

private:
    struct Node {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;
    };

    std::unique_ptr<Node> _head {std::make_unique<Node>()};
    Node *_tail {_head.get()};

    mutable std::mutex _head_mutex;
    mutable std::mutex _tail_mutex;
    
    std::condition_variable _condition;

    friend class Queue_v2_helper<Queue_v2<T>>;
};

template <typename Q>
auto Queue_v2_helper<Q>::get_tail_copy() {
    auto impl = static_cast<Q*>(this);
    std::lock_guard<std::mutex> _ {impl->_tail_mutex};
    return impl->_tail;
}

template <typename Q>
auto Queue_v2_helper<Q>::pop_head() {
    auto impl = static_cast<Q*>(this);
    std::lock_guard<std::mutex> _ {impl->_head_mutex};
    auto t = get_tail_copy();
    // dummy
    if(impl->_head.get() == t) {
        return decltype(impl->_head){nullptr};
    }
    auto h = std::move(impl->_head);
    // detach h and h->next
    impl->_head = std::move(h->next);
    return h;
}


template <typename T>
void Queue_v2<T>::push(T value) {
    auto dummy = std::make_unique<Node>();
    auto data = std::make_shared<T>(std::move(value));

    // no lock here

    std::lock_guard<std::mutex> _ {_tail_mutex};
    _tail->data = std::move(data);
    _tail->next = std::move(dummy);
    _tail = _tail->next.get();
}

template <typename T>
bool Queue_v2<T>::try_pop(T &out) {
    auto old = this->pop_head();
    if(!old) return false;

    // no lock here

    out = std::move(*old->data);
    return true;
}

template <typename T>
auto Queue_v2<T>::try_pop() -> std::shared_ptr<T> {
    auto old = this->pop_head();
    if(!old) return nullptr;
    auto p = std::move(old->data);
    return p;
}












// 一个测试用对象，构造慢，但是移动快
// （为了抬举v2的性能真是煞费苦心啊，v1版容器使用T而非shared_ptr<T>，内存分配友好对于简单的类型其实性能更优）
struct HugeObject {
    std::vector<int> x;
    size_t y;
    HugeObject(size_t i): x(100), y(i) {}
};

// 通过构造一个生产者消费者模型来测试
// 生产者会把HugeObject放入并发队列Q中，消费者则把Q中的元素pop出来，并放入到res中
// res用于判断实现的正确性
template <template <typename> typename Q>
void testQueue() {
    Q<HugeObject> q;
    auto provider = [&q](size_t count, size_t start) {
        for(size_t i {start}; i < count + start; ++i) {
            q.push(HugeObject{i});
        }
    };
    // TODO atomic size_t
    std::vector<HugeObject> res;
    std::mutex res_mtx;
    auto consumer = [&](size_t count) {
        for(size_t i {}; i < count;) {
            // v2版本没写wait实现
            // auto p = (i & 1) ? q.try_pop() : q.wait_and_pop();
            auto p = q.try_pop(); 
            if(!p) continue;
            std::lock_guard<std::mutex> _ {res_mtx};
            res.emplace_back(std::move(*p));
            ++i;
        }
    };

    constexpr size_t count = 5e6;
    constexpr size_t consumers = 5;
    constexpr size_t providers = 2;
    static_assert(count % consumers == 0);
    static_assert(count % providers == 0);

    std::vector<std::thread> consumer_threads;
    std::vector<std::thread> provider_threads;
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
    testQueue<Queue_v2>();
    return 0;
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
