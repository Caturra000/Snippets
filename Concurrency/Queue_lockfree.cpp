#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstddef>
#include "Tagged_ptr.hpp"
#include "Freelist.hpp"
#include "util/timer.hpp"

// 实现lockfree queue
// paper: https://dl.acm.org/doi/10.1145/248052.248106

template <typename T, typename Alloc_of_T = std::allocator<T>>
class Queue {
public:
    struct Node;
    using Node_ptr = Tagged_ptr<Node>;

    using Requirements = std::enable_if_t<std::is_trivial_v<T> /* && TODO */>;

    struct alignas(64) Node {
        T data;
        std::atomic<Node_ptr> next;

        template <typename ...Args> Node(Args &&...args)
            : data(std::forward<Args>(args)...)
        {
            next.store(Node_ptr{nullptr}, std::memory_order_relaxed);
        }
    };
    using Node_alloc = typename std::allocator_traits<Alloc_of_T>
                        :: template rebind_alloc<Node>;

public:
    Queue();
    ~Queue();

public:
    // 在MS queue原来的算法中，push必然成功
    // 但是freelist可能会失败，因此仍保留bool接口
    template <typename ...Args>
    bool push(Args &&...);

    std::optional<T> pop();

private:
    Freelist<Node, Node_alloc> _pool;

    // 各自独占cacheline

    alignas(64) std::atomic<Node_ptr> _head;
    alignas(64) std::atomic<Node_ptr> _tail;
};

template <typename T, typename A>
Queue<T, A>::Queue() {
    Node *dummy = _pool.allocate();
    if(!dummy) throw std::bad_alloc();
    _pool.construct(dummy);

    Node_ptr tagged_dummy {dummy, 0x5a5a};
    _head.store(tagged_dummy);
    _tail.store(tagged_dummy);
}

template <typename T, typename A>
Queue<T, A>::~Queue() {
    while(pop());
}

template <typename T, typename A>
template <typename ...Args>
bool Queue<T, A>::push(Args &&...args) {
    auto node_ptr = _pool.allocate();
    if(!node_ptr) return false;
    _pool.construct(node_ptr, std::forward<Args>(args)...);

    Tagged_ptr<Node> node {node_ptr};

    // 假设Tx是当前线程，Ty是第二个线程
    // （虽然是lockfree，但算法中有实际进展的是任意2条线程）
    for(;;) {
        auto tail = _tail.load(std::memory_order_acquire);
        auto next = tail->next.load(std::memory_order_acquire);

        auto tail2 = _tail.load(std::memory_order_acquire);
        // 需要tag比较，确保tail和next是一致的
        if(tail != tail2) continue;

        // L1能确保原子性的append（需要后续L2 CAS的保证）
        // 因为MS Queue存在不断链的性质
        if(next == nullptr) {                                                   // L1
            node.set_tag(next.next_tag());
            // failed意味着有其他线程至少提前完成了L2，导致当前L1条件违反
            if(tail->next.compare_exchange_weak(next, node,
                    std::memory_order_release, std::memory_order_relaxed)) {    // L2
                node.set_tag(tail.next_tag());
                // 我认为这一步不可能failed
                // Tx执行L2成功，意味着Ty即使能执行到L1，也不能CAS到L2，
                // 因为L1的条件是整个链表中唯一存在的，
                // 而此时符合next==nullptr的node并没有发布出去（L3）
                //
                // Note: 不应使用timed lock（避免spurious failure），因此不是weak
                //
                // Note: 实际L3上可能返回false，见L4，其实是在不同线程上完成相同的工作
                _tail.compare_exchange_strong(tail, node,
                    std::memory_order_release, std::memory_order_relaxed);      // L3
                return true;
            }
        // Ty完成了L2甚至L3
        } else {
            next.set_tag(tail.next_tag());
            // _tail在MS Queue中是指向最后一个或者倒数第二个结点
            // - 指向最后一个结点就不必多说了，常规数据结构的形态
            // - 指向倒数第二个是因为存在Ty完成了L2，但是L3尚未完成
            // 
            // 如果Ty L3未完成，失败方会“推波助澜”，帮助Tx完成tail向前移动到最后一个结点的操作
            // （next如果可见了，那也是唯一确定的）
            // 这样可以提高并发吞吐
            _tail.compare_exchange_strong(tail, next,
                std::memory_order_release, std::memory_order_relaxed);          // L4
        }
    }
}

template <typename T, typename A>
std::optional<T> Queue<T, A>::pop() {
    for(;;) {
        auto head = _head.load(std::memory_order_acquire);
        auto tail = _tail.load(std::memory_order_acquire);
        auto next = head->next.load(std::memory_order_acquire);

        // 保证head tail next一致性
        auto head2 = _head.load(std::memory_order_acquire);
        if(head != head2) continue;

        if(head.get_ptr() == tail.get_ptr()) {
            // is dummy
            if(!next) return std::nullopt;
            next.set_tag(tail.next_tag());
            // Ty push进行中，Tx推波助澜
            _tail.compare_exchange_strong(tail, next,
                std::memory_order_release, std::memory_order_relaxed);
        } else {
            if(!next) continue;
            // copy
            auto opt = std::make_optional<T>(next->data);
            next.set_tag(head.next_tag());

            if(_head.compare_exchange_weak(head, next,
                    std::memory_order_release, std::memory_order_relaxed)) {
                auto old_head_ptr = head.get_ptr();
                _pool.destroy(old_head_ptr);
                _pool.deallocate(old_head_ptr);
                return opt;
            }
        }
    }
}



struct Trivial_object {
    // std::vector<int> x;
    size_t y;
    Trivial_object() = default;
    Trivial_object(size_t i): y(i) {}
};

template <template <typename> typename Q>
void testQueue() {
    Q<Trivial_object> q;
    Q<Trivial_object> receiver;
    auto provider = [&q](size_t count, size_t start) {
        for(size_t i {start}; i < count + start; ++i) {
            q.push(Trivial_object{i});
        }
    };
    auto consumer = [&](size_t count) {
        for(size_t i {}; i < count;) {
            auto p = q.pop(); 
            if(!p) continue;
            while(!receiver.push(std::move(*p)));
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
    std::vector<Trivial_object> res;
    for(std::optional<Trivial_object> opt; opt = receiver.pop(); res.emplace_back(std::move(*opt)));
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
    testQueue<Queue>();
    return 0;
}