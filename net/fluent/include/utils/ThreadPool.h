#ifndef __FLUENT_UTILS_THREAD_POOL_H__
#define __FLUENT_UTILS_THREAD_POOL_H__
#include <bits/stdc++.h>
namespace fluent {

class ThreadPool {
public:
    explicit ThreadPool(int size) { while(size--) run(); }
    ~ThreadPool();

    template <typename Func, typename ...Args>
    void execute(Func &&functor, Args &&...args);

private:
    void run();

    struct Data {
        std::mutex _mutex;
        std::condition_variable _condition;
        bool _stop {false};
        std::queue<std::function<void()>> _tasks;
    };
    std::shared_ptr<Data> _data {std::make_shared<Data>()};
};

inline ThreadPool::~ThreadPool() {
    if(auto data = _data) {
        {
            std::lock_guard<std::mutex> _ { data->_mutex};
            data->_stop = true;
        }
        data->_condition.notify_all();
    }
}

template <typename Func, typename ...Args>
inline void ThreadPool::execute(Func &&functor, Args &&...args) {
    {
        std::lock_guard<std::mutex> _ { _data->_mutex};
        // _data->_tasks.emplace(Callable::make(
        //     std::forward<Func>(functor), std::forward<Args>(args)...));
        _data->_tasks.emplace([=] {functor(std::move(args)...);});
    }
    _data->_condition.notify_one();
}

inline void ThreadPool::run() {
    std::thread { [this] {
        auto data = _data; // lifecycle
        std::unique_lock<std::mutex> lock {data->_mutex};
        for(;;) {
            if(!data->_tasks.empty()) {
                auto task = std::move(data->_tasks.front());
                data->_tasks.pop();
                lock.unlock();
                if(task) task();
                lock.lock();
            } else if(data->_stop) {
                break;
            } else {
                data->_condition.wait(lock);
            }
        }
    }}.detach();
}

} // fluent
#endif