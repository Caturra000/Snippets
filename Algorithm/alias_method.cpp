#include <bits/stdc++.h>

class Alias {
public:
    Alias(std::vector<double> probabilities)
    {
        precondition(probabilities);
        std::tie(_probs, _alias) = make_alias_table(std::move(probabilities));
    }

    /// @brief  Just simply make a random O(1) choice.
    /// @return Index.
    size_t make() {
    #if 0 // Simple method.
        std::uniform_int_distribution dis0N {0ul, _alias.size() - 1};
        std::uniform_real_distribution dis01 {0.0, 1.0};
        // TODO: single distribution is enough.
        size_t i = dis0N(_gen);
        double p = dis01(_gen);
    #else // Faster (~10%) but harder to write correctly.
        std::uniform_real_distribution dis01 {0.0, 1.0};
        double x = dis01(_gen);
        size_t i = _alias.size() * x; // round down
        double p = _alias.size() * x - i;
    #endif
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
        std::vector<std::tuple<size_t, double>> U[3];
        enum u_type {OVERFULL=0, FULL, UNDERFULL};

        for(size_t i = 0; i < probabilities.size(); ++i) {
            auto p = probabilities[i];
            u_type who = p > 1 ? OVERFULL : UNDERFULL;

            if(is_one(p)) [[unlikely]] {
                who = FULL;
                // Optional, but make less buggy code.
                // NOTE: FULL actually has no alias index.
                K[i] = i;
            }

            U[who].emplace_back(i, p);
        }

        while(!U[OVERFULL].empty() && !U[UNDERFULL].empty()) {
            /// Calculate.
            auto [over_i, over_p] = pop(U[OVERFULL]);
            auto [under_i, under_p] = pop(U[UNDERFULL]);
            over_p -= (1 - under_p);
            K[under_i] = over_i;

            /// Reinsert.
            U[FULL].emplace_back(under_i, under_p);
            u_type who = over_p > 1 ? OVERFULL : UNDERFULL;
            if(is_one(over_p)) [[unlikely]] {
                who = FULL;
                K[over_i] = over_i;
            }
            U[who].emplace_back(over_i, over_p);
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

        // They are all FULL.
        std::vector<double> real_u(N);
        for(auto &&[i, p] : U[FULL]) real_u[i] = p;
        return std::make_tuple(real_u, K);
    }

    // 1. size() > 0
    // 2. \sum probabilities == 1.0
    static
    void precondition(const auto &probabilities) noexcept {
        assert(!probabilities.empty());
        double p_sum = 0;
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
        double delta = (distribution - probabilities[i])/probabilities[i];
        std::cout << i << ":\t" << distribution << "\t(d=" << delta << ")\n";
    }

}

/***********
0:      0.399998        (d=-4.425e-06)
1:      0.300031        (d=0.0001041)
2:      0.200013        (d=6.57e-05)
3:      0.0499534       (d=-0.0009322)
4:      0.050004        (d=8.02e-05)
***********/
