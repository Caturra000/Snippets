// 只是看论文简单复现的布谷鸟过滤器
// 实现上并不考虑所有情况

#include <bits/stdc++.h>

/// @brief A simple fixed-size cuckoo filter.
/// @tparam T Type of items.
/// @tparam N Size of cuckoo filter. Must Be a power of 2.
template <typename T, size_t N>
class Cuckoo {
/// Interfaces.
public:
    Cuckoo() noexcept;

    /// @brief Add an item to cuckoo filter.
    /// @param item Any string.
    /// @return Maybe false if no enough space.
    bool add(const T &item) noexcept;

    /// @brief Remove an item from cuckoo filter.
    /// @param item Any added string.
    void remove(const T &item) noexcept;

    /// @brief Lookup an item from cuckoo filter.
    /// @param item Any string.
    /// @return True if found.
    bool lookup(const T &item) const noexcept;

private:
    /// @brief Fingerprint is 16-bit fixed.
    using fbits = uint16_t;

    /// @brief
    /// UNSAFE magic code for checking null entry in bucket.
    /// Obviously, this is not suitable for a normal cuckoo filter,
    /// but it does make cuckoo algorithm simpler.
    constexpr static fbits magic_code = 0x5a5a;

    /// @brief STL-like npos.
    constexpr static size_t npos = -1;

/// Core functions.
private:
    /// @brief Generate a fingerprint for x.
    /// @return A 16-bit fingerprint.
    /// @todo Add any-bit fingerprint.
    fbits fingerprint(const T &x) const noexcept;

    /// @brief Hash value of fingerprint.
    /// @param f Fingerprint.
    /// @return A 64-bit remapped value, but restricted below N.
    size_t hash(fbits f) const noexcept;

    size_t hash(const T &x) const noexcept;

/// Helper functions.
private:
    /// @brief buckets[i] has fingerprint f?
    /// @return Return entry index or npos.
    size_t bucket_has(size_t i, fbits f) const noexcept;

    /// @brief buckets[i] has a hole?
    /// @return Return hole-entry index or npos.
    size_t bucket_has_hole(size_t i) const noexcept;

    /// @brief Add an entry to bucket <del>randomly</del>.
    /// @param i Bucket index.
    /// @param e Entry index.
    /// @param f Entry value (fingerprint).
    void add_entry(size_t i, size_t e, fbits f) noexcept;

    /// @brief Reset buckets[i][e] to null.
    void reset_entry(size_t i, size_t e) noexcept;

    /// @brief Return buckets[i][random()].
    fbits& any_entry(size_t i) noexcept;

    /// @todo Currently _buckets is fixed size.
    void rehash() {}

private:
    /// 4-way buckets.
    /// TODO: Configurable multiway buckets.
    fbits _buckets[N][4];
};



template <typename T, size_t N>
inline Cuckoo<T, N>::Cuckoo() noexcept {
    static_assert(N == (N&-N), "Cuckoo needs a power of 2.");
    for(auto &bucket : _buckets) {
        std::fill(std::begin(bucket), std::end(bucket), magic_code);
    }
}

template <typename T, size_t N>
inline bool Cuckoo<T, N>::add(const T &item) noexcept {
    fbits f = fingerprint(item);
    size_t i1 = hash(item);
    size_t i2 = i1 ^ hash(f);
    size_t itable[2] {i1, i2};
    static bool s_round {};
    /// Similar to a random choice.
    if(s_round ^= 1) std::swap(itable[0], itable[1]);
    /// About variables:
    /// i - bucket index.
    /// e - entry index.
    /// entry - entry reference.
    for(auto i : itable) {
        auto e = bucket_has_hole(i);
        if(e != npos) [[likely]] {
            add_entry(i, e, f);
            return true;
        }
    }
    size_t i = itable[0];
    for(size_t retry = 100; retry--;) {
        auto &entry = any_entry(i);
        std::swap(f, entry);
        /// Cuckoo f has been added to entry.
        /// Now we need to solve entry relocation (swapped into f).
        i ^= hash(f);
        auto next_e = bucket_has_hole(i);

        if(next_e != npos) {
            add_entry(i, next_e, f);
            return true;
        }
    }
    /// Hashtable is considered full.
    return false;
}

template <typename T, size_t N>
inline void Cuckoo<T, N>::remove(const T &item) noexcept {
    fbits f = fingerprint(item);
    size_t i1 = hash(item);
    size_t i2 = i1 ^ hash(f);
    size_t itable[] {i1, i2};
    for(auto i : itable) {
        if(auto e = bucket_has(i, f); e != npos) {
            reset_entry(i, e);
            return;
        }
    }
    assert(false);
}

template <typename T, size_t N>
inline bool Cuckoo<T, N>::lookup(const T &item) const noexcept {
    fbits f = fingerprint(item);
    size_t i1 = hash(item);
    size_t i2 = i1 ^ hash(f);
    return (bucket_has(i1, f) != npos) || (bucket_has(i2, f) != npos);
}

template <typename T, size_t N>
inline typename Cuckoo<T, N>::fbits Cuckoo<T, N>::fingerprint(const T &x) const noexcept {
    constexpr size_t fbits_shift = sizeof(fbits) << 3;
    constexpr size_t bitmask = ~(static_cast<size_t>(-1) << fbits_shift);
    return std::hash<T>()(x) & bitmask;
}

template <typename T, size_t N>
inline size_t Cuckoo<T, N>::hash(fbits f) const noexcept {
    /// FIXME: std::hash<*trivial*> returns f directly.
    return std::hash<fbits>()(f) % N;
}

template <typename T, size_t N>
inline size_t Cuckoo<T, N>::hash(const T &x) const noexcept {
    return std::hash<std::string>()(x) % N;
}

template <typename T, size_t N>
inline size_t Cuckoo<T, N>::bucket_has(size_t i, fbits f) const noexcept {
    for(size_t j = 0; j < 4; ++j) {
        if(_buckets[i][j] == f) return j;
    }
    return npos;
}

template <typename T, size_t N>
inline size_t Cuckoo<T, N>::bucket_has_hole(size_t i) const noexcept {
    for(size_t j = 0; j < 4; ++j) {
        if(_buckets[i][j] == magic_code) return j;
    }
    return npos;
}

template <typename T, size_t N>
inline void Cuckoo<T, N>::add_entry(size_t i, size_t e, fbits f) noexcept {
    assert(_buckets[i][e] == magic_code);
    _buckets[i][e] = f;
}

template <typename T, size_t N>
inline void Cuckoo<T, N>::reset_entry(size_t i, size_t e) noexcept {
    _buckets[i][e] = magic_code;
}

template <typename T, size_t N>
inline typename Cuckoo<T, N>::fbits& Cuckoo<T, N>::any_entry(size_t i) noexcept {
    static std::default_random_engine re{std::random_device{}()};
    std::uniform_int_distribution<size_t> dis{0, 3};
    size_t e = dis(re);
    assert(_buckets[i][e] != magic_code);
    return _buckets[i][e];
}


/*************************************************************************/

void simple() {
    std::vector<std::string> strings {
        "jintianxiao",
        "midaobi",
        "lema"
    };

    std::vector<std::string> not_included {
        "jintianxiaomidaobilema",
        "fenbushiruanzongxiancaozuoxitong"
    };

    Cuckoo<std::string, 128> filter;
    size_t err_n {};
    for(auto &&s : strings) filter.add(s);
    for(auto &&n : not_included) err_n += !!filter.lookup(n);
    assert(err_n == 0);
    filter.remove(strings[1]);
    err_n += filter.lookup(strings[1]);
    assert(err_n == 0);

    std::cout << "OK: simple test." << std::endl;
}

auto random_generate(size_t icounts, size_t ecounts) {
    std::unordered_set<std::string> unique_data;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis_length(20, 50);
    std::uniform_int_distribution<char> dis_alpha('a', 'z');

    for(auto n = icounts + ecounts; n--;) {
        std::string str;
        size_t length = dis_length(gen);
        while(length--) str += dis_alpha(gen);
        unique_data.emplace(std::move(str));
    }
    std::vector<std::string> included, excluded;
    for(auto &&v : unique_data) {
        if(icounts) icounts--, included.emplace_back(std::move(v));
        else excluded.emplace_back(std::move(v));
    }
    return std::make_tuple(std::move(included), std::move(excluded));
}

template <size_t N = 64>
void fuzzy(const char *test_name,
           const std::vector<std::string> &included,
           const std::vector<std::string> &excluded)
{
    auto cuckoo_ptr = std::make_unique<Cuckoo<std::string, N>>();
    auto &cuckoo = *cuckoo_ptr.get();

    size_t add_failed = 0;
    for(auto &&inc : included) add_failed += !cuckoo.add(inc);
    /// Space is limited. Try to use larger N.
    /// We MUST ensure no failed add() in this test case.
    if(add_failed) {
        std::cerr << "add failed: " << add_failed << std::endl;
    }

    assert("N is too small." && !add_failed);

    size_t lookup_failed = 0;
    for(auto &&inc : included) lookup_failed += !cuckoo.lookup(inc);
    /// lookup_failed MUST MUST MUST be 0.
    if(lookup_failed) {
        std::cerr << "lookup failed: " << lookup_failed << std::endl;
    }

    assert("Bug, bug never changes." && !lookup_failed);

    size_t false_positives = 0;
    for(auto &&exc : excluded) false_positives += cuckoo.lookup(exc);
    /// It's ok. If a filter returns true, it may be false.
    if(false_positives) {
        std::cerr << "false positives: " << (1.0 * false_positives / excluded.size() * 100) << "%\n";
    }

    for(size_t i = 0; i < 10; ++i) {
        cuckoo.remove(included[i]);
        /// It's ok. Hash values may be conflict.
        if(cuckoo.lookup(included[i])) {
            std::cerr << "AIEEEEE! A NINJA!? WHY THERE'S A NINJA HERE!?" << std::endl;
        }
    }

    std::cout << "OK: fuzzy test (" << test_name << ")\n.";
    std::cout << "=======================" << std::endl;
}

int main() {
    simple();

    /// Generated by chatgpt.
    std::vector<std::string> foods {"Pizza","Sushi","Burger","Pasta","Tacos","Ice Cream","Chicken Curry","Salad","Steak","Pancakes","Dim Sum","Lasagna","Chocolate Cake","Shrimp Scampi","Ramen","Apple Pie","Fish and Chips","Burrito","Caesar Salad","Chicken Wings","Pad Thai","BBQ Ribs","Nachos","Fried Rice","Clam Chowder","Gyro","Creme Brulee","Lobster Bisque","Hot Dog","Mashed Potatoes","Pho","Goulash","Croissant","Macaroni and Cheese","Guacamole","French Toast","Biryani","Popcorn","Omelette","Tikka Masala","Croque Monsieur","Ceviche","Spring Rolls","Deviled Eggs","Tiramisu","Peking Duck","Quiche","Gumbo","Chicken Parmesan","Cinnamon Roll","Lobster Roll","Ratatouille","Chicken Quesadilla","Hamburger","Chicken Salad","Cabbage Rolls","Empanadas","Fried Chicken","Shish Kebab","Pumpkin Pie","Tom Yum Soup","Cannoli","Lobster Thermidor","Poutine","Cucumber Roll","Philly Cheesesteak","Chicken Tandoori","Miso Soup","Buffalo Wings","Risotto","Chimichanga","Falafel","Onion Rings","Caesar Wrap","Egg Drop Soup","Fettuccine Alfredo","Tandoori Chicken","Chili Con Carne","Spanakopita","Beef Stroganoff","Caprese Salad","Chocolate Mousse","Reuben Sandwich","Chicken Noodle Soup","Paella","Cobb Salad","Lobster Mac and Cheese","Beef and Broccoli","Baklava","Chicken Enchiladas","Minestrone Soup","Bagel with Lox and Cream Cheese","Ratatouille","Muffuletta","Gazpacho","Tom Kha Gai","Shrimp Po' Boy","Avocado Toast","Croque Madame","Clam Bake"};
    std::vector<std::string> people {"Emily Johnson","Alexander Smith","Olivia Davis","Ethan Wilson","Sophia Taylor","Mason Jones","Ava Miller","Liam Anderson","Isabella Brown","Noah Martinez","Emma Davis","Aiden Thomas","Mia Johnson","Lucas White","Harper Lee","Logan Harris","Amelia Clark","Jackson Moore","Grace Turner","Carter Robinson"};

    fuzzy("GPT", foods, people);

    for(auto _ : "jintianxiaomidaobilema") {
        std::ignore = _;
        constexpr size_t cuckoo_size = 1 << 20;
        constexpr size_t istrings = cuckoo_size / 10;
        constexpr size_t estrings = cuckoo_size / 10;
        auto [included, excluded] = random_generate(istrings, estrings);

        fuzzy<cuckoo_size>("random", included, excluded);
    }
}
