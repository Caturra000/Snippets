#include <bits/stdc++.h>

// This class is not for generic usage.
// SIZE must be a power of 2.
template <typename T, size_t SIZE>
struct Bounded_mpmc {
    constexpr static size_t MASK = SIZE - 1;
    constexpr static size_t SHIFT = std::bit_width(SIZE)-1;
    struct Slot {
        alignas(64) std::atomic<size_t> seq {};
        T val;
    };
    std::array<Slot, SIZE> _buf;
    alignas(64) std::atomic<size_t> _in {};
    alignas(64) std::atomic<size_t> _out {};

    size_t seq(int t) { return t >> SHIFT << 1; }
    size_t idx(int t) { return t & MASK; }

    void push(T elem) {
        size_t in = _in.fetch_add(1, std::memory_order_acq_rel);
        auto &slot = _buf[idx(in)];
        while(seq(in) != slot.seq.load(std::memory_order_acquire));
        slot.val = std::move(elem);
        slot.seq.store(seq(in) + 1, std::memory_order_release);
    }

    T pop() {
        auto out = _out.fetch_add(1, std::memory_order_acq_rel);
        auto &slot = _buf[idx(out)];
        while(seq(out) + 1 != slot.seq.load(std::memory_order_acquire));
        auto val = std::move(slot.val);
        slot.seq.store(seq(out) + 2, std::memory_order_release);
        return val;
    }
};

int main() {
    // Note: stack size is limited.
    using MPMC = Bounded_mpmc<size_t, 1 << 20>;
    auto mpmc = std::make_unique<MPMC>();
    constexpr size_t count = 1e8;
    constexpr size_t producer_size = 10;
    constexpr size_t consumer_size = 10;
    static_assert(count % producer_size == 0);
    static_assert(count % consumer_size == 0);
    std::atomic<size_t> g_value = 0;
    using namespace std::views;
    auto produce = [&](size_t first_value, size_t partial_count) {
        for(auto c : iota(first_value) | take(partial_count)) {
            mpmc->push(c);
        }
    };
    auto consume = [&](int partial_count) {
        size_t sum = 0;
        for(auto c : iota(0, partial_count)) {
            auto val = mpmc->pop();
            sum += val;
        }
        g_value += sum;
    };

    {
        std::jthread producers[producer_size];
        std::jthread consumers[consumer_size];

        size_t producer_first_value = 1;
        constexpr size_t producer_take = count / producer_size;
        for(auto &&p : producers) {
            p = std::jthread{produce, producer_first_value, producer_take};
            producer_first_value += producer_take;
        }

        constexpr size_t consumer_take = count / consumer_size;
        for(auto &&c : consumers) {
            c = std::jthread{consume, consumer_take};
        }
    }

    constexpr auto sum = (count + 1) * count / 2;
    // Checkpoint
    assert(g_value == sum);
    std::cout << g_value << std::endl;
}
