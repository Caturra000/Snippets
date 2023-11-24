#include <bits/stdc++.h>

struct Alias {

    Alias(std::vector<double> probabilities)
    {
        precondition(probabilities);
        std::tie(_probs, _alias) = make_alias_table(std::move(probabilities));
    }

    /// @brief  Just simply make a random choice.
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
    static
    auto make_alias_table(std::vector<double> probabilities)
    -> std::tuple< std::vector<double>, std::vector<size_t> >
    {
        constexpr auto EPS = std::numeric_limits<double>::epsilon();
        const auto N = probabilities.size();
        for(auto &p : probabilities) p *= N;
        std::vector<size_t> k(N, N);
        /// {index, probability}.
        std::vector<std::tuple<size_t, double>> u[3];
        enum u_type {OVERFULL=0, FULL, UNDERFULL};

        for(size_t i = 0; i < probabilities.size(); ++i) {
            auto p = probabilities[i];
            u_type who = p > 1 ? OVERFULL : UNDERFULL;

            if(fabs(p-1) < EPS) [[unlikely]] {
                who = FULL;
                k[i] = i;
            }

            u[who].emplace_back(i, p);
        }

        while(!u[OVERFULL].empty() && !u[UNDERFULL].empty()) {
            auto over = u[OVERFULL].back();
            auto under = u[UNDERFULL].back();
            u[OVERFULL].pop_back();
            u[UNDERFULL].pop_back();

            /// Calculate.
            auto &[over_i, over_p] = over;
            auto &[under_i, under_p] = under;
            over_p -= (1 - under_p);
            k[under_i] = over_i;

            /// Reinsert.
            u[FULL].emplace_back(under_i, under_p);
            u_type who = over_p > 1 ? OVERFULL : UNDERFULL;
            if(fabs(over_p - 1) < EPS) [[unlikely]] {
                who = FULL;
                k[over_i] = over_i;
            }
            u[who].emplace_back(over_i, over_p);
        }

        /// I hate floating points.
        auto corner_case = [&](auto &ulist) {
            while(!ulist.empty()) {
                auto elem = ulist.back();
                ulist.pop_back();
                auto &[i, p] = elem;
                assert(fabs(p-1) < EPS);
                k[i] = i;
                u[FULL].emplace_back(i, p);
            }
        };

        corner_case(u[OVERFULL]);
        corner_case(u[UNDERFULL]);

        /// Sorted by index.
        std::ranges::sort(u[FULL]);
        std::vector<double> real_u;
        for(auto &&[_, p] : u[FULL]) real_u.emplace_back(p);
        return std::make_tuple(real_u, k);
    }

    void precondition(const auto &probabilities) noexcept {
        assert(!probabilities.empty());
        constexpr auto EPS = std::numeric_limits<double>::epsilon();
        double p_sum = 0;
        for(auto p : probabilities) p_sum += p;
        assert(fabs(p_sum - 1) < EPS);
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
        std::cout <<  i << ":\t" << distribution << '\n';
    }
}
