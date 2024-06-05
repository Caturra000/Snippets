#include <bits/stdc++.h>

class Solution {
public:
    int evaluate(std::string e) {
        ctl.s = std::move(e);
        ctl.pos = 0;
        return expression();
    }

private:
    struct {
        std::string s;
        int pos;
        char peek() { return s[pos]; }
        char pop() { return s[pos++]; }
        void pop(size_t step) { pos += step; }
        void eat(char c = ' ') { while(peek() == c) pop(); }
        bool _accept(char c) { return peek() == c; }
        bool _accept(std::invocable<char> auto cond) { return cond(peek()); }
        bool accept(auto ...all) { return (_accept(all) || ...); }
        bool first_accept(auto ...all) { return eat(), accept(all...); }
    } ctl;

    auto defer(auto f) {
        return std::shared_ptr<void>(nullptr, std::move(f));
    }

    using Int = long long;
    using Symbol_table = std::deque<std::unordered_map<std::string, Int>>;
    Symbol_table symbols;

    // e -> NUM | v | (let (v e)+ e) | (add e e) | (multi e e)
    Int expression() {
        if(auto num = number()) return *num;
        if(auto var = variant()) return *var;

        ctl.pop(); // (
        auto _ = defer([&](...) { ctl.pop(); }); // )

        switch(ctl.peek()) {
            case 'l': return ctl.pop(3), let();
            case 'a': return ctl.pop(3), expression() + expression();
            case 'm': return ctl.pop(5), expression() * expression();
        }
        return {};
    }

    std::optional<Int> number() {
        if(!ctl.first_accept(::isdigit, '-', '+')) return {};
        Int ans = 0;
        bool negative = ctl.accept('-', '+') && ctl.pop() == '-';
        while(ctl.accept(::isdigit)) {
            ans = ans * 10 + (ctl.pop() - '0');
        }
        return negative ? -ans : ans;
    }

    Int variant_value(const std::string &name) {
        for(auto &scope : symbols) {
            if(scope.contains(name)) return scope[name];
        }
        return {}; // Unreachable.
    }

    std::string variant_name() {
        std::string name;
        while(ctl.accept(::isdigit, ::isalpha)) {
            name += ctl.pop();
        }
        return name;
    }

    std::optional<Int> variant() {
        if(!ctl.first_accept(::isalpha)) return {};
        return variant_value(variant_name());
    }

    Int let() {
        auto &local = symbols.emplace_front();
        auto _ = defer([&](...) {symbols.pop_front();});
        // v ' ' e ' '
        while(ctl.first_accept(::isalpha)) {
            auto name = variant_name();
            ctl.eat();

            if(ctl.peek() == ')') {
                return variant_value(name);
            }

            local[name] = expression();
        }
        return expression();
    }
};

int main() {
    Solution s;
    std::cout << s.evaluate("(let y 2 x (let q 2 z y 200) (add x (multi x y)))") << std::endl;
    return 0;
}
