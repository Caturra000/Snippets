#include <atomic>
#include <mutex>

class Singleton {
public:
    static Singleton* instance() {
        // 为了满足后面ptr = new Singleton();并release的happens-before关系（可能由其它线程操作）
        // 你需要acquire-release或者consume-release
        // 总之你能针对instance构造前后提供一种跨线程的偏序就好了
        auto ptr = _instance.load(std::memory_order_acquire);
        if(!ptr) {
            std::lock_guard<std::mutex> _ {_mutex};
            // _instance本身的获取是原子的，因此直接relaxed
            ptr = _instance.load(std::memory_order_relaxed);
            if(!ptr) {
                ptr = new Singleton();
                _instance.store(ptr, std::memory_order_release);
            }
        }
        // 因为要求dereference必须要happens-after于其它线程的初始化
        // 所以创建实例后要release，并且dereference前要acquire/consume
        return ptr;
    }

private:
    static std::atomic<Singleton*> _instance;
    static std::mutex _mutex;
};

std::atomic<Singleton*> Singleton::_instance {nullptr};
std::mutex Singleton::_mutex;

int main() {
    auto instance = Singleton::instance();
    return 0;
}