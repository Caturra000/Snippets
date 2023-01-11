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
