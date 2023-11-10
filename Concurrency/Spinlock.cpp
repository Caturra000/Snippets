#include <bits/stdc++.h>

namespace V1 {
struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    // 问题：x86下即使全用relaxed似乎也没问题？
    bool tryLock() { return !flag.test_and_set(std::memory_order_acquire); }
    void lock() { while(flag.test_and_set(std::memory_order_acquire)); }
    void unlock() { flag.clear(std::memory_order_release); }
};
} // V1

// 思路来自：https://www.boost.org/doc/libs/1_78_0/doc/html/atomic/usage_examples.html
// No atomic flag
namespace V2 {
struct Spinlock {
    enum State {LOCKED, UNLOCKED};
    std::atomic<State> _state {UNLOCKED};

    // 如果之前是LOCKED，那就一直busy-wait
    void lock() { while(_state.exchange(LOCKED, std::memory_order_acquire) == LOCKED);}
    void unlock() { _state.store(UNLOCKED, std::memory_order_release); }
};
} // V2

// TTAS version: no always-acquire
namespace V3 {
struct Spinlock {
    enum State {LOCKED, UNLOCKED};
    std::atomic<State> _state {UNLOCKED};

    void lock() {
        while(_state.exchange(LOCKED, std::memory_order_acquire) == LOCKED) {
            while(_state.load(std::memory_order_relaxed) == LOCKED);
        }
    }
    void unlock() { _state.store(UNLOCKED, std::memory_order_release); }
};
} // V3

// TTAS version 2: +fence
namespace V4 {
struct Spinlock {
    enum State {LOCKED, UNLOCKED};
    std::atomic<State> _state {UNLOCKED};

    void lock() {
        while(_state.exchange(LOCKED, std::memory_order_acquire) == LOCKED) {
            while(_state.exchange(LOCKED, std::memory_order_relaxed) == LOCKED);
            std::atomic_thread_fence(std::memory_order_acquire);
            break;
        }
    }
    void unlock() { _state.store(UNLOCKED, std::memory_order_release); }
};
} // V4

// TTAS version 3: +yield
namespace V5 {
struct Spinlock {
    enum State {LOCKED, UNLOCKED};
    std::atomic<State> _state {UNLOCKED};

    void lock() {
        while(_state.exchange(LOCKED, std::memory_order_acquire) == LOCKED) {
            while(_state.exchange(LOCKED, std::memory_order_relaxed) == LOCKED)
                std::this_thread::yield();
            std::atomic_thread_fence(std::memory_order_acquire);
            break;
        }
    }
    void unlock() { _state.store(UNLOCKED, std::memory_order_release); }
};
} // V5

// V1 + yield
namespace V6 {
struct Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

    bool tryLock() { return !flag.test_and_set(std::memory_order_acquire); }
    void lock() { while(flag.test_and_set(std::memory_order_acquire)) std::this_thread::yield(); }
    void unlock() { flag.clear(std::memory_order_release); }
};
} // V6

constexpr size_t contention_level = 10;
constexpr size_t contention_times = 10000000;

template <typename Lock>
void incr(size_t &value, size_t count) {
    static Lock s_lock;
    while(count--) {
        std::lock_guard<Lock> _ {s_lock};
        value++;
    }
}

template <typename Lock>
void test() {
    using namespace std::chrono;
    auto start = steady_clock::now();
    std::thread threads[contention_level];
    size_t value {};
    for(auto &&t : threads) t = std::thread{incr<Lock>, std::ref(value), contention_times};
    for(auto &&t : threads) t.join();
    auto end = steady_clock::now();
    assert(value == contention_times * contention_level);
    std::cout << duration_cast<milliseconds>(end - start).count() << "\tms\n";
}

int main() {
    auto test_runner = [&]<typename ...Ts>() {
        (test<Ts>(), ...);
    };

    test_runner.template operator()<
        V1::Spinlock,
        V2::Spinlock,
        V3::Spinlock,
        V4::Spinlock,
        V5::Spinlock,
        V6::Spinlock
    >();
}

// 简单的测试结果
// 11219   ms
// 10014   ms
// 11925   ms
// 9007    ms
// 4110    ms
// 4635    ms
// 看来是TTAS+yield最优
// atomic flag不仅难用，还不占性能优势
