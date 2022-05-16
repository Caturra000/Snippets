#include <bits/stdc++.h>

// 参考libuv的思路，写了个更复杂点的线程池
// 并且用工地英语来提高逼格
// !!未经测试

// Features:
// 1. fixed-size pool (real parallelism by default), while tasks can be overcommited (and run immediately if needed)
// 2. automatically blocking slow IO for further short-running jobs
// 3. lazy initialized
// 4. (TODO) async-notification via event loop (pipe-fd?)
// 5. (TODO) prepare callback for slow IO
class ThreadPool {
public:
    struct FastIO {};
    // TODO SlowIO == CPU
    struct SlowIO {};
    struct CPU {};

public:
    template <typename Policy, typename F, typename ...Args>
    void submit(F &&func, Args &&...args) {
        std::call_once(_init, &ThreadPool::lazyInit, this);

        std::function<void()> realTask =
            std::bind(std::forward<F>(func), std::forward<Args>(args)...);
        /*synchronized*/ {
            std::lock_guard<std::mutex> guard {_data->mutex};
            if(std::is_same<Policy, SlowIO>::value) {
                _data->pendingTasks.emplace(std::move(realTask));
                realTask = [this] { runSlowMessage() };
            }
            _data->tasks.emplace(std::move(realTask));
        }
        // TODO reduce frequency of notification
        _data->cv.notify_one();
    }

public:
    ThreadPool() = default;
    ~ThreadPool() { _data->stop = true; _data->cv.notify_all(); };
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = default;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = default;

private:
    void lazyInit();
    void runSlowMessage();
    void consume();

private:

    // data shared for all threads, should be protected by shared pointer
    struct Data {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<std::function<void()>> tasks;
        // low priority
        std::queue<std::function<void()>> pendingTasks;
        // size of threadpool: (typically) fixed size, but we can scale up for high priority context
        size_t size;
        size_t runningSlowIO;
        size_t runningSlowIOThreshold;
        // Note: cannot FORCE stop, all submitted jobs will be done even if stopping
        bool stop;
    };

    std::shared_ptr<Data> _data;
    // once flag for lazy initialization
    std::once_flag _init;
};


inline void ThreadPool::lazyInit() {
    _data = std::make_shared<Data>();
    _data->size = std::thread::hardware_concurrency();
    _data->runningSlowIO = 0;
    _data->runningSlowIOThreshold = std::max<size_t>(1, _data->size >> 1);
    _data->stop = false;
    for(auto iter = _data->size; iter--;) {
        std::thread {&ThreadPool::consume, this}.detach();
    }
}

inline void ThreadPool::runSlowMessage() {
    std::function<void()> task;
    {
        // another copy of `_data` is held by worker
        std::lock_guard<std::mutex> guard {_data->mutex};
        if(_data->runningSlowIO > _data->runningSlowIOThreshold) {
            // try it later
            _data->tasks.emplace(&ThreadPool::runSlowMessage, this);
            return;
        }
        size_t pendings = _data->pendingTasks.size();
        if(pendings == 0) {
            return;
        }
        _data->runningSlowIO++;
        task = std::move(_data->pendingTasks.front());
        _data->pendingTasks.pop();
        if(pendings > 1) {
            _data->tasks.emplace(&ThreadPool::runSlowMessage, this);
        }
    }
    if(task) task();
    std::lock_guard<std::mutex> guard {_data->mutex};
    _data->runningSlowIO--;


}

inline void ThreadPool::consume() {
    auto data = _data;
    if(data == nullptr) return;
    // keep waiting if there are too many running slow-IO (and current queue's head is also slow-IO).
    auto rejectSlowIO = [&] {
        // FIXME data->tasks.size() == 1
        return data->tasks.size() == 1 && !data->pendingTasks.empty()
            && data->runningSlowIO > data->runningSlowIOThreshold;
    };
    std::unique_lock<std::mutex> guard {data->mutex};
    for(;;) {
        while(!data->stop && (data->tasks.empty() || rejectSlowIO())) {
            data->cv.wait(guard);
        }
        // it is guaranteed that all tasks will be done even if (data->stop == true)
        if(!data->tasks.empty()) {
            auto task = std::move(data->tasks.front());
            data->tasks.pop();
            guard.unlock();
            if(task) task();
            guard.lock();
        } else if(data->stop) {
            break;
        }
    }
}
