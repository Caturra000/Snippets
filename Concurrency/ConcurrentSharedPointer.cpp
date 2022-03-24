#include <bits/stdc++.h>
using namespace std;

// 思路来自：https://www.boost.org/doc/libs/1_78_0/doc/html/atomic/usage_examples.html
// 注意这里只是做一个草稿，代码是不可用的

class ConcurrentSharedPointer {
public:

private:
    std::atomic<int> *_referenceCount;

    void acquire() {
        // 这里的假设是_referenceCount是肯定存在的
        // 不需要acquire是因为肯定在此前的其它地方已经提供过(?)
        // - 我觉得应该结合release()来看，RMW能看到最新值，因此acquire用relaxed没问题
        // - 反正这个值也就给release看一下
        _referenceCount->fetch_add(1, std::memory_order_relaxed);
    }

    void release() {
        // 可以直接在fetch_sub用acq_rel，这样就不需要fence
        // 不过会有额外性能损失，因为还没降到0也付出了acquire成本
        if(_referenceCount->fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete _referenceCount;
        }
    }
};