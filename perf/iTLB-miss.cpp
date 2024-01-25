#include <bits/stdc++.h>

// 一个反复横跳的序列下标生成器
consteval size_t monkey_seqid(auto I, auto Bound) {
    return (I & 1) ? Bound - I : I;
}

template <size_t I>
[[gnu::optimize("O0")]]
void func() {}

int main() {
    constexpr size_t Bound = 1e3;

    constexpr auto default_seq = std::make_index_sequence<Bound>{};
    constexpr auto monkey_seq = []<size_t ...Is>(std::index_sequence<Is...>) {
        return std::index_sequence<monkey_seqid(Is, Bound)...>{};
    }(default_seq);

    constexpr auto callseq = []<size_t ...Is>(std::index_sequence<Is...>) {
        (func<Is>(), ...);
    };

    // 引导生成顺序代码，需要反汇编确认代码段是按顺序生成func[0, Bound]
    callseq(default_seq);

    constexpr size_t loop = 1e6;
    for(auto l{loop}; l--;) {
        // 每次都尽可能反复横跳，使其miss
        callseq(monkey_seq);
    }
}
