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

int main() {
    Queue<size_t> q;
    auto provider = [&q](size_t count, size_t start) {
        for(size_t i {start}; i < count + start; ++i) {
            q.push(i);
        }
    };
    // TODO atomic size_t
    std::vector<size_t> res;
    std::mutex res_mtx;
    auto consumer = [&](size_t count) {
        for(size_t i {}; i < count;) {
            auto p = (i & 1) ? q.try_pop() : q.wait_and_pop(); 
            if(!p) continue;
            std::lock_guard<std::mutex> _ {res_mtx};
            res.push_back(*p);
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
    auto sum = std::accumulate(res.begin(), res.end(), 0);
    for(size_t i {}; i < count; ++i) sum -=i;
    if(sum) {
        throw std::runtime_error("sum error");
    }

    // check [0, count)
    std::sort(res.begin(), res.end());
    std::for_each(res.begin(), res.end(), [v=0](auto elem) mutable {
        if(elem != v) {
            throw std::runtime_error("elem error");
        }
        v++;
    });

    std::cout << "ok" << std::endl;
    return 0;
}
