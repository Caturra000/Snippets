// 只是看论文简单复现的布谷鸟过滤器
// 实现上并不考虑所有情况

#include <bits/stdc++.h>

template <size_t N>
class Cuckoo {
/// Interfaces.
public:
    Cuckoo();

    /// @brief Add an item to cuckoo filter.
    /// @param item Any string.
    /// @return Maybe false if no enough space.
    bool add(const std::string &item);

    /// @brief Remove an item from cuckoo filter.
    /// @param item Any added string.
    void remove(const std::string &item);

    /// @brief Lookup an item from cuckoo filter.
    /// @param item Any string.
    /// @return True if found.
    bool lookup(const std::string &item);

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
    fbits fingerprint(const std::string &x);

    /// @brief Hash value of fingerprint.
    /// @param f Fingerprint.
    /// @return A 64-bit remapped value, but restricted below N.
    size_t hash(fbits f);

    size_t hash(const std::string &x);

/// Helper functions.
private:
    /// @brief buckets[i] has fingerprint f?
    /// @return Return entry index or npos.
    size_t bucket_has(size_t i, fbits f);

    /// @brief buckets[i] has a hole?
    /// @return Return hole-entry index or npos.
    size_t bucket_has_hole(size_t i);

    /// @brief Add an entry to bucket <del>randomly</del>.
    /// @param i Bucket index.
    /// @param e Entry index.
    /// @param f Entry value (fingerprint).
    void add_entry(size_t i, size_t e, fbits f);

    /// @brief Reset buckets[i][e] to null.
    void reset_entry(size_t i, size_t e);

    /// @brief Return buckets[i][random()].
    fbits& any_entry(size_t i);

    /// @todo Currently _buckets is fixed size.
    void rehash() {}

private:
    /// 4-way buckets.
    /// TODO: rename to _hashtable.
    fbits _buckets[N][4];

/// For debug.
public:
    size_t _debug_relocation {};

    double scan_room() {
        size_t n = 0;
        auto cond = [](auto f) { return f != magic_code; };
        for(auto &&bucket : _buckets) {
            n += std::ranges::count_if(bucket, cond);
        }
        return static_cast<double>(n) / N / 4 * 100;
    }
};



template <size_t N>
inline Cuckoo<N>::Cuckoo() {
    static_assert(N == (N&-N), "Cuckoo needs a power of 2.");
    for(auto &bucket : _buckets) {
        std::fill(std::begin(bucket), std::end(bucket), magic_code);
    }
}

template <size_t N>
inline bool Cuckoo<N>::add(const std::string &item) {
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

        _debug_relocation++;

        if(next_e != npos) {
            add_entry(i, next_e, f);
            return true;
        }
    }
    /// Hashtable is considered full.
    return false;        
}

template <size_t N>
inline void Cuckoo<N>::remove(const std::string &item) {
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

template <size_t N>
inline bool Cuckoo<N>::lookup(const std::string &item) {
    fbits f = fingerprint(item);
    size_t i1 = hash(item);
    size_t i2 = i1 ^ hash(f);
    return (bucket_has(i1, f) != npos) || (bucket_has(i2, f) != npos);
}

template <size_t N>
inline typename Cuckoo<N>::fbits Cuckoo<N>::fingerprint(const std::string &x) {
    return std::hash<std::string>()(x) & ~(-1 << 16);
}

template <size_t N>
inline size_t Cuckoo<N>::hash(fbits f) {
    /// FIXME: std::hash<*trivial*> returns f directly.
    return std::hash<fbits>()(f) % N;
}

template <size_t N>
inline size_t Cuckoo<N>::hash(const std::string &x) {
    return std::hash<std::string>()(x) % N;
}

template <size_t N>
inline size_t Cuckoo<N>::bucket_has(size_t i, fbits f) {
    for(size_t j = 0; j < 4; ++j) {
        if(_buckets[i][j] == f) return j;
    }
    return npos;
}

template <size_t N>
inline size_t Cuckoo<N>::bucket_has_hole(size_t i) {
    for(size_t j = 0; j < 4; ++j) {
        if(_buckets[i][j] == magic_code) return j;
    }
    return npos;
}

template <size_t N>
inline void Cuckoo<N>::add_entry(size_t i, size_t e, fbits f) {
    assert(_buckets[i][e] == magic_code);
    _buckets[i][e] = f;
}

template <size_t N>
inline void Cuckoo<N>::reset_entry(size_t i, size_t e) {
    _buckets[i][e] = magic_code;
}

template <size_t N>
inline typename Cuckoo<N>::fbits& Cuckoo<N>::any_entry(size_t i) {
    size_t e = rand() & 3;
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

    Cuckoo<128> filter;
    size_t err_n {};
    for(auto &&s : strings) filter.add(s);
    for(auto &&n : not_included) err_n += !!filter.lookup(n);
    assert(err_n == 0);
    filter.remove(strings[1]);
    err_n += filter.lookup(strings[1]);
    assert(err_n == 0);
}

auto random_generate(size_t counts) {
    std::set<std::string> unique_data;
    for(auto n = counts; n--;) {
        std::string gen;
        int length = rand() % 30 + 1; // [1, 30]
        while(length--) gen += rand() % 26 + 'a';
        unique_data.emplace(std::move(gen));
    }
    return std::vector<std::string> {unique_data.begin(), unique_data.end()};
}

template <size_t N = 64>
void fuzzy(const std::vector<std::string> &included,
           const std::vector<std::string> &excluded)
{
    auto cuckoo_ptr = std::make_unique<Cuckoo<N>>();
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
}

int main() {
    simple();

    /// Generated by chatgpt.
    std::vector<std::string> foods {"Pizza","Sushi","Burger","Pasta","Tacos","Ice Cream","Chicken Curry","Salad","Steak","Pancakes","Dim Sum","Lasagna","Chocolate Cake","Shrimp Scampi","Ramen","Apple Pie","Fish and Chips","Burrito","Caesar Salad","Chicken Wings","Pad Thai","BBQ Ribs","Nachos","Fried Rice","Clam Chowder","Gyro","Creme Brulee","Lobster Bisque","Hot Dog","Mashed Potatoes","Pho","Goulash","Croissant","Macaroni and Cheese","Guacamole","French Toast","Biryani","Popcorn","Omelette","Tikka Masala","Croque Monsieur","Ceviche","Spring Rolls","Deviled Eggs","Tiramisu","Peking Duck","Quiche","Gumbo","Chicken Parmesan","Cinnamon Roll","Lobster Roll","Ratatouille","Chicken Quesadilla","Hamburger","Chicken Salad","Cabbage Rolls","Empanadas","Fried Chicken","Shish Kebab","Pumpkin Pie","Tom Yum Soup","Cannoli","Lobster Thermidor","Poutine","Cucumber Roll","Philly Cheesesteak","Chicken Tandoori","Miso Soup","Buffalo Wings","Risotto","Chimichanga","Falafel","Onion Rings","Caesar Wrap","Egg Drop Soup","Fettuccine Alfredo","Tandoori Chicken","Chili Con Carne","Spanakopita","Beef Stroganoff","Caprese Salad","Chocolate Mousse","Reuben Sandwich","Chicken Noodle Soup","Paella","Cobb Salad","Lobster Mac and Cheese","Beef and Broccoli","Baklava","Chicken Enchiladas","Minestrone Soup","Bagel with Lox and Cream Cheese","Ratatouille","Muffuletta","Gazpacho","Tom Kha Gai","Shrimp Po' Boy","Avocado Toast","Croque Madame","Clam Bake"};
    std::vector<std::string> people {"Emily Johnson","Alexander Smith","Olivia Davis","Ethan Wilson","Sophia Taylor","Mason Jones","Ava Miller","Liam Anderson","Isabella Brown","Noah Martinez","Emma Davis","Aiden Thomas","Mia Johnson","Lucas White","Harper Lee","Logan Harris","Amelia Clark","Jackson Moore","Grace Turner","Carter Robinson"};

    fuzzy(foods, people);

    constexpr size_t cuckoo_size = 1 << 20;
    constexpr size_t random_generate_size = cuckoo_size / 10;
    std::vector<std::string> included = random_generate(random_generate_size);
    std::vector<std::string> excluded {included.begin(), included.begin() + included.size() / 10};
    for(auto &&s : excluded) s += '_';

    fuzzy<cuckoo_size>(included, excluded);
}
