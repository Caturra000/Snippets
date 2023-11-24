// 推荐阅读：
// https://en.wikipedia.org/wiki/Alias_method
// https://zhuanlan.zhihu.com/p/111885669

#include <bits/stdc++.h>

class Alias {
public:
    Alias(std::vector<double> probabilities) {
        precondition(probabilities);
        std::tie(_probs, _alias) = make_alias_table(std::move(probabilities));
    }

    /// @brief  Just simply make a random O(1) choice.
    /// @return Index.
    size_t make() {
        std::uniform_real_distribution dis01 {0.0, 1.0};
        double x = dis01(_gen);
        size_t i = _alias.size() * x; // round down
        double p = _alias.size() * x - i;
        return p < _probs[i] ? i : _alias[i];
    }

    /// @deprecated
    /// __Simple method__ to make a random O(1) choice.
    /// Use make() instead, which is faster (~10%) but harder to write correctly.
    size_t make(decltype(std::ignore)...) {
        std::uniform_int_distribution dis0N {0ul, _alias.size() - 1};
        std::uniform_real_distribution dis01 {0.0, 1.0};
        size_t i = dis0N(_gen);
        double p = dis01(_gen);
        return p < _probs[i] ? i : _alias[i];
    }

private:
    // O(n) construction.
    static
    auto make_alias_table(std::vector<double> probabilities)
    -> std::tuple< std::vector<double>, std::vector<size_t> >
    {
        const auto N = probabilities.size();
        for(auto &p : probabilities) p *= N;

        // K: alias table.
        // U: {index, probability} table.
        std::vector<size_t> K(N, N /*uninitialized*/);
        enum u_type {OVERFULL, FULL, UNDERFULL, UTYPE_MAX};
        std::vector<std::tuple<size_t, double>> U[UTYPE_MAX];
        auto get_type = [&](size_t i, double p) {
            u_type type = p > 1 ? OVERFULL : UNDERFULL;
            if(is_one(p)) [[unlikely]] {
                type = FULL;
                // Optional, but make less buggy code.
                // NOTE: FULL or OVERFULL->FULL actually has no alias index.
                K[i] = i;
            }
            return type;
        };

        for(size_t i = 0; i < probabilities.size(); ++i) {
            auto p = probabilities[i];
            u_type type = get_type(i, p);
            U[type].emplace_back(i, p);
        }

        while(!U[OVERFULL].empty() && !U[UNDERFULL].empty()) {
            /// Calculate.
            auto [over_i, over_p] = pop(U[OVERFULL]);
            auto [under_i, under_p] = pop(U[UNDERFULL]);
            over_p -= (1.0 - under_p);
            K[under_i] = over_i;

            /// Reinsert.
            U[FULL].emplace_back(under_i, under_p);
            u_type type = get_type(over_i, over_p);
            U[type].emplace_back(over_i, over_p);
        }

        /// I hate floating points.
        auto corner_case = [&](auto &ulist) {
            while(!ulist.empty()) {
                auto [i, p] = pop(ulist);
                assert(is_one(p));
                K[i] = i;
                U[FULL].emplace_back(i, p);
            }
        };

        corner_case(U[OVERFULL]);
        corner_case(U[UNDERFULL]);

        // Now they are all FULL.
        std::vector<double> full_u(N);
        for(auto &&[i, p] : U[FULL]) full_u[i] = p;
        return std::make_tuple(full_u, K);
    }

    // 1. size() > 0
    // 2. \sum probabilities == 1.0
    static
    void precondition(const std::vector<double> &probabilities) noexcept {
        assert(!probabilities.empty());
        [[maybe_unused]] double p_sum = 0;
        for(auto p : probabilities) p_sum += p;
        assert(is_one(p_sum));
    }

    static
    bool is_one(double x) noexcept {
        constexpr auto EPS = std::numeric_limits<double>::epsilon();
        return fabs(x - 1) < EPS;
    }

    template <typename T> static
    T pop(std::vector<T> &ulist) {
        auto elem = std::move(ulist.back());
        ulist.pop_back();
        return elem;
    }

private:
    std::random_device _rd;
    std::mt19937 _gen{_rd()};

    std::vector<double> _probs;
    std::vector<size_t> _alias;
};

int main() {
    [[maybe_unused]]
    std::vector items         {1,   2,   3,   4,    5};
    std::vector probabilities {0.4, 0.3, 0.2, 0.05, 0.05};

    assert(items.size() == probabilities.size());

    Alias alias(probabilities);

    std::vector<size_t> counts(items.size());

    constexpr size_t test_round = 1e8;

    for(auto n = test_round; n--;) {
        auto i = alias.make();
        counts[i]++;
    }

    for(size_t i = 0; i < counts.size(); ++i) {
        double distribution = counts[i];
        distribution /= test_round;
        double delta = (distribution - probabilities[i]) / probabilities[i] * 100;
        std::cout << i << ":\t" << distribution << "\t(d=" << delta << "%)\n";
    }

}

/***********
0:      0.399994        (d=-0.0015175%)
1:      0.299967        (d=-0.01113%)
2:      0.200001        (d=0.0007%)
3:      0.0500284       (d=0.05686%)
4:      0.0500096       (d=0.01926%)
***********/
