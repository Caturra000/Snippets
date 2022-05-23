#include <bits/stdc++.h>

// 参考6.S081单变量实现
class SharedMutex {
public:
    void lock();
    void unlock();
    void lockShared();
    void unlockShared();

private:
    // n == 0：未上锁
    // n == -1：被一个writer持有
    // n > 0：被n个reader持有
    std::atomic<int> _n {0};
};

inline void SharedMutex::lockShared() {
    for(;;) {
        int old = _n.load(std::memory_order_relaxed);
        if(old < 0) continue;
        if(_n.compare_exchange_weak(old, old+1,
            std::memory_order_acquire, std::memory_order_relaxed)) return;
    }
}

inline void SharedMutex::unlockShared() {
    for(;;) {
        int old = _n.load(std::memory_order_relaxed);
        if(old <= 0) continue;
        if(_n.compare_exchange_weak(old, old-1,
            std::memory_order_release, std::memory_order_relaxed)) return;
    }
}

inline void SharedMutex::lock() {
    for(;;) {
        int old = 0;
        if(_n.compare_exchange_strong(old, -1,
            std::memory_order_acquire, std::memory_order_relaxed)) return;
    }
}

inline void SharedMutex::unlock() {
    for(;;) {
        int old = -1;
        if(_n.compare_exchange_strong(old, 0,
            std::memory_order_release, std::memory_order_relaxed)) return;
    }
}

int main() {
    int val = 0;
    SharedMutex mutex;
    constexpr static size_t COUNT = 1e4;
    auto writer = [&] {
        for(int i = 0; i < COUNT; ++i) {
            mutex.lock();
            val++;
            mutex.unlock();
        }
    };
    // test deadlock
    auto nop = [&] {
        for(int i = 0; i < COUNT; ++i) {
            mutex.lockShared();
            mutex.unlockShared();
        }
    };
    std::vector<std::thread> workers;
    for(int count = 8; count--;) workers.emplace_back(writer);
    for(int count = 8; count--;) workers.emplace_back(nop);
    for(auto &&worker: workers) worker.join();
    std::cout << val << std::endl;
    return 0;
}
