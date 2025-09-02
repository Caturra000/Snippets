#include <atomic>
#include <array>
#include <optional>
#include <thread>
#include <cassert>
#include <iostream>
#include <ranges>
#include <memory>

// A lock-free, single-producer-single-consumer and fixed-size FIFO implementation.
//
// That is:
// Only one thread may call push().
// Only one thread may call pop().
//
// T: type of elements.
// SIZE: size of the allocated buffer. MUST be power of 2.
//
// NOTE: this class is focused on lock-free algorithm only, not for generic usage.
template <typename T, size_t SIZE>
struct Fifo {
    std::array<T, SIZE> _buffer;
    alignas(64) std::atomic<size_t> _in {};
    alignas(64) std::atomic<size_t> _out {};

    Fifo() { static_assert(SIZE > 1 && !(SIZE & SIZE-1), "Read the comments!"); }

    // For mod computation.
    inline constexpr static size_t MASK = SIZE - 1;

    bool push(T elem) {
        // Only producer can modify _in.
        size_t in = _in.load(std::memory_order_relaxed);
        size_t next_in = in+1 & MASK;
        if(next_in == _out.load(std::memory_order_acquire)) {
            return false;
        }
        _buffer[in] = std::move(elem);
        // Let consumer know your change.
        _in.store(next_in, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        size_t in = _in.load(std::memory_order_acquire);
        size_t out = _out.load(std::memory_order_relaxed);
        if(in == out) return std::nullopt;
        auto opt = std::make_optional<T>(std::move(_buffer[out]));
        _out.store(out+1 & MASK, std::memory_order_release);
        return opt;
    }
};

int main() {
    // Note: stack size is limited.
    using FF = Fifo<size_t, 1 << 20>;
    auto ff = std::make_unique<FF>();
    constexpr size_t count = 1e8;
    constexpr size_t first_value = 1;
    size_t g_value = 0;
    using namespace std::views;
    auto produce = [&] {
        for(auto c : iota(first_value) | take(count)) {
            while(!ff->push(c));
        }
    };
    auto consume = [&] {
        for(auto c : iota(first_value) | take(count)) {
            std::optional<int> opt;
            while(!(opt = ff->pop()));
            auto val = opt.value();
            static size_t prev = 0;
            // Checkpoint #1
            assert(prev+1 == val);
            prev = val;
            g_value += val;

        }
    };
    std::jthread producer {produce};
    std::jthread consumer {consume};
    producer = {};
    consumer = {};
    constexpr auto sum = (count + 1) * count / 2;
    // Checkpoint #2
    assert(g_value == sum);
    std::cout << g_value << std::endl;
}
