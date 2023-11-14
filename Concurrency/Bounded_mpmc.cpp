#include <bits/stdc++.h>

// This toy class is NOT for generic usage.
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

    // std::memory_order_too_long is toooooo long.
    constexpr static auto acq_rel = std::memory_order_acq_rel;
    constexpr static auto acquire = std::memory_order_acquire;
    constexpr static auto release = std::memory_order_release;
    constexpr static auto relaxed = std::memory_order_relaxed;

    void push(T elem) {
        size_t in = _in.fetch_add(1, acq_rel);
        auto &slot = _buf[idx(in)];
        while(seq(in) != slot.seq.load(acquire));
        slot.val = std::move(elem);
        slot.seq.store(seq(in) + 1, release);
    }

    T pop() {
        auto out = _out.fetch_add(1, acq_rel);
        auto &slot = _buf[idx(out)];
        while(seq(out) + 1 != slot.seq.load(acquire));
        auto val = std::move(slot.val);
        slot.seq.store(seq(out) + 2, release);
        return val;
    }

    bool try_push(T elem) {
        size_t in = _in.load(acquire);
        for(;;) {
            auto &slot = _buf[idx(in)];
            if(seq(in) != slot.seq.load(acquire)) {
                size_t old_in = in;
                // Advance and update local `in`.
                in = _in.load(acquire);
                if(old_in == in) return false;
            } else if(_in.compare_exchange_weak(in, in+1, acq_rel, relaxed)) {
                slot.val = std::move(elem);
                slot.seq.store(seq(in) + 1, release);
                return true;
            }
        }
    }

    std::optional<T> try_pop() {
        size_t out = _out.load(acquire);
        for(;;) {
            auto &slot = _buf[idx(out)];
            if(seq(out) + 1 != slot.seq.load(acquire)) {
                size_t old_out = out;
                out = _out.load(acquire);
                if(old_out == out) return std::nullopt;
            } else if(_out.compare_exchange_weak(out, out+1, acq_rel, relaxed)) {
                auto opt = std::make_optional<T>(std::move(slot.val));
                slot.seq.store(seq(out) + 2, release);
                return opt;
            }
        }
    }
};

int main() {
    // Note: stack size is limited.
    using MPMC = Bounded_mpmc<size_t, 1 << 20>;
    auto mpmc = std::make_unique<MPMC>();
    constexpr size_t count = 1e8;
    constexpr size_t producer_size = 10;
    constexpr size_t consumer_size = 10;
    static_assert(producer_size > 1 && count % producer_size == 0);
    static_assert(consumer_size > 1 && count % consumer_size == 0);
    struct Property {};
    constexpr struct Blocking: Property {} blocking;
    constexpr struct Non_blocking: Property {} non_blocking;
    std::atomic<size_t> g_value = 0;
    using namespace std::views;
    auto produce = [&]<std::derived_from<Property> T>(size_t first_value, size_t partial_count, T) {
        for(auto c : iota(first_value) | take(partial_count)) {
            if constexpr (std::is_same_v<T, Blocking>) {
                mpmc->push(c);
            } else {
                while(!mpmc->try_push(c));
            }
        }
    };
    auto consume = [&]<std::derived_from<Property> T>(int partial_count, T) {
        size_t sum = 0;
        for(auto c : iota(0, partial_count)) {
            std::ignore = c;
            if constexpr (std::is_same_v<T, Blocking>) {
                auto val = mpmc->pop();
                sum += val;
            } else {
                std::optional<size_t> opt;
                while(!(opt = mpmc->try_pop()));
                sum += *opt;
            }
        }
        g_value += sum;
    };

    {
        std::jthread producers[producer_size];
        std::jthread consumers[consumer_size];

        size_t producer_first_value = 1;
        constexpr size_t producer_take = count / producer_size;
        bool rr = false;
        for(auto &&p : producers) {
            auto make_thread = [&](auto blocking_property) {
                return std::jthread{produce, producer_first_value, producer_take, blocking_property};
            };
            p = (rr ^= 1) ? make_thread(blocking) : make_thread(non_blocking);
            producer_first_value += producer_take;
        }

        constexpr size_t consumer_take = count / consumer_size;
        for(auto &&c : consumers) {
            auto make_thread = [&](auto blocking_property) {
                return std::jthread{consume, consumer_take, blocking_property};
            };
            c = (rr ^= 1) ? make_thread(blocking) : make_thread(non_blocking);
        }
    }

    constexpr auto sum = (count + 1) * count / 2;
    // Checkpoint
    assert(g_value == sum);
    std::cout << g_value << std::endl;
}
