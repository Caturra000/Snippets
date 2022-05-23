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
    // 注意初始化，老是忘
    std::atomic<int> _n {0};
};

inline void SharedMutex::lockShared() {
    int old;
    do {
        old = _n.load(std::memory_order_relaxed);
        if(old < 0) continue;
    // 这里如果失败的话其实old应该也会得到一个更新值（expected = *this），没必要下一次old = ...，
    // 但是仍要考虑首次old < 0的情况，因此还是决定写繁琐点
    //
    // 内存模型上CAS如果成功那就是RMW，失败则load（为了满足expected得到实际值）
    // 见：https://en.cppreference.com/w/cpp/atomic/atomic/compare_exchange
    // 如果只论x86的话，memory_order怎么写都是对的，因为要成功必须RMW，RMW必然有类似LOCK前缀的保证
    // 目前只是简单的遵循acquire-release，后面看看能不能直接仅一个fence就足够了？
    } while(!_n.compare_exchange_strong(old, old+1, std::memory_order_acquire, std::memory_order_relaxed));
}

inline void SharedMutex::unlockShared() {
    int old;
    do {
        old = _n.load(std::memory_order_relaxed);
        if(old <= 0) continue;
    } while(!_n.compare_exchange_strong(old, old-1, std::memory_order_release, std::memory_order_relaxed));
}

inline void SharedMutex::lock() {
    int old;
    do {
        old = 0;
        // old = _n.load(std::memory_order_relaxed);
        // if(old != 0) continue;
    } while(!_n.compare_exchange_strong(old, -1, std::memory_order_acquire, std::memory_order_relaxed));
}

inline void SharedMutex::unlock() {
    int old;
    do {
        old = -1;
        // old = _n.load(std::memory_order_relaxed);
        // if(old != -1) continue;
    } while(!_n.compare_exchange_strong(old, 0, std::memory_order_release, std::memory_order_relaxed));
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
