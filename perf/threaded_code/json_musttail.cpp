// json_musttail_benchmark.cpp
// Clang: clang++ -std=c++23 -O3 -march=native -o bench json_musttail_benchmark.cpp -lbenchmark -lpthread
// GCC:   g++-14 -std=c++23 -O3 -march=native -o bench json_musttail_benchmark.cpp -lbenchmark -lpthread

#include <benchmark/benchmark.h>
#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <iostream>
#include <cstring>

// ============================================================================
// Musttail 兼容宏 - 注意 GCC 和 Clang 的差异
// ============================================================================
#if defined(__clang__)
    #define MUSTTAIL [[clang::musttail]]
    #define HAS_MUSTTAIL 1
#elif defined(__GNUC__) && __GNUC__ >= 14
    #define MUSTTAIL [[gnu::musttail]]
    #define HAS_MUSTTAIL 1
#else
    #define MUSTTAIL
    #define HAS_MUSTTAIL 0
    #warning "musttail not supported"
#endif

// ============================================================================
// Common Types
// ============================================================================

enum class JsonToken : uint8_t {
    Invalid = 0, ObjectStart, ObjectEnd, ArrayStart, ArrayEnd,
    String, Number, True, False, Null, Colon, Comma, End, Continue, Count
};

enum class CharClass : uint8_t {
    Whitespace = 0, Digit, Alpha, Quote, Colon, Comma,
    OpenBrace, CloseBrace, OpenBracket, CloseBracket,
    Minus, Plus, Dot, Backslash, Exponent, Other, Count
};

const char* token_name(JsonToken t) {
    static const char* names[] = {
        "Invalid", "ObjectStart", "ObjectEnd", "ArrayStart", "ArrayEnd",
        "String", "Number", "True", "False", "Null", "Colon", "Comma", 
        "End", "Continue"
    };
    return names[static_cast<int>(t)];
}

// ============================================================================
// Part 1: Character Classification
// ============================================================================

namespace char_classify {

// Lookup table (baseline)
constexpr std::array<CharClass, 256> make_char_table() {
    std::array<CharClass, 256> t{};
    for (int i = 0; i < 256; ++i) t[i] = CharClass::Other;
    t[' '] = t['\t'] = t['\n'] = t['\r'] = CharClass::Whitespace;
    for (int c = '0'; c <= '9'; ++c) t[c] = CharClass::Digit;
    for (int c = 'a'; c <= 'z'; ++c) t[c] = CharClass::Alpha;
    for (int c = 'A'; c <= 'Z'; ++c) t[c] = CharClass::Alpha;
    t['e'] = t['E'] = CharClass::Exponent;
    t['"'] = CharClass::Quote;
    t[':'] = CharClass::Colon;
    t[','] = CharClass::Comma;
    t['{'] = CharClass::OpenBrace;
    t['}'] = CharClass::CloseBrace;
    t['['] = CharClass::OpenBracket;
    t[']'] = CharClass::CloseBracket;
    t['-'] = CharClass::Minus;
    t['+'] = CharClass::Plus;
    t['.'] = CharClass::Dot;
    t['\\'] = CharClass::Backslash;
    return t;
}
inline constexpr auto kCharTable = make_char_table();

[[gnu::noinline]]
CharClass classify_table(char c) {
    return kCharTable[static_cast<unsigned char>(c)];
}

// ============================================================================
// Original DirectThreaded (无 musttail) - 作为对照
// ============================================================================
using ClassifyFunc = CharClass(*)(char);

CharClass h_whitespace(char) { return CharClass::Whitespace; }
CharClass h_digit(char) { return CharClass::Digit; }
CharClass h_alpha(char) { return CharClass::Alpha; }
CharClass h_exponent(char) { return CharClass::Exponent; }
CharClass h_quote(char) { return CharClass::Quote; }
CharClass h_colon(char) { return CharClass::Colon; }
CharClass h_comma(char) { return CharClass::Comma; }
CharClass h_open_brace(char) { return CharClass::OpenBrace; }
CharClass h_close_brace(char) { return CharClass::CloseBrace; }
CharClass h_open_bracket(char) { return CharClass::OpenBracket; }
CharClass h_close_bracket(char) { return CharClass::CloseBracket; }
CharClass h_minus(char) { return CharClass::Minus; }
CharClass h_plus(char) { return CharClass::Plus; }
CharClass h_dot(char) { return CharClass::Dot; }
CharClass h_backslash(char) { return CharClass::Backslash; }
CharClass h_other(char) { return CharClass::Other; }

constexpr std::array<ClassifyFunc, 256> make_func_table() {
    std::array<ClassifyFunc, 256> t{};
    for (int i = 0; i < 256; ++i) t[i] = h_other;
    t[' '] = t['\t'] = t['\n'] = t['\r'] = h_whitespace;
    for (int c = '0'; c <= '9'; ++c) t[c] = h_digit;
    for (int c = 'a'; c <= 'z'; ++c) t[c] = h_alpha;
    for (int c = 'A'; c <= 'Z'; ++c) t[c] = h_alpha;
    t['e'] = t['E'] = h_exponent;
    t['"'] = h_quote; t[':'] = h_colon; t[','] = h_comma;
    t['{'] = h_open_brace; t['}'] = h_close_brace;
    t['['] = h_open_bracket; t[']'] = h_close_bracket;
    t['-'] = h_minus; t['+'] = h_plus; t['.'] = h_dot; t['\\'] = h_backslash;
    return t;
}
inline constexpr auto kFuncTable = make_func_table();

[[gnu::noinline]]
CharClass classify_direct(char c) {
    return kFuncTable[static_cast<unsigned char>(c)](c);
}

// ============================================================================
// Musttail 版本 - 关键：musttail 必须是函数的最后一条语句
// 解决方案：使用三元运算符选择下一个 handler，避免 if 分支
// ============================================================================

struct ClassifyState {
    const char* cur;
    const char* end;
    uint64_t sum;
};

using ClassifyHandler = void(*)(ClassifyState&);

// 终止函数 - 什么都不做
void classify_done(ClassifyState&) {}

// 前向声明
void classify_dispatch(ClassifyState& s);

// 所有 handler 的通用模式：处理当前字符，然后用 musttail 跳转
// 关键：使用三元运算符选择 handler，让 musttail 成为最后一条语句

void classify_m_whitespace(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Whitespace);
    ++s.cur;
    // 使用三元运算符，让 musttail 成为函数的唯一出口
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_digit(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Digit);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_alpha(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Alpha);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_exponent(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Exponent);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_quote(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Quote);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_colon(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Colon);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_comma(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Comma);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_open_brace(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::OpenBrace);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_close_brace(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::CloseBrace);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_open_bracket(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::OpenBracket);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_close_bracket(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::CloseBracket);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_minus(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Minus);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_plus(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Plus);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_dot(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Dot);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_backslash(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Backslash);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

void classify_m_other(ClassifyState& s) {
    s.sum += static_cast<uint8_t>(CharClass::Other);
    ++s.cur;
    ClassifyHandler next = (s.cur < s.end) ? classify_dispatch : classify_done;
    MUSTTAIL return next(s);
}

// Handler 表
inline ClassifyHandler classify_handlers[256];
inline bool classify_init_flag = false;

void init_classify_handlers() {
    if (classify_init_flag) return;
    for (int i = 0; i < 256; ++i) classify_handlers[i] = classify_m_other;
    classify_handlers[' '] = classify_handlers['\t'] = 
    classify_handlers['\n'] = classify_handlers['\r'] = classify_m_whitespace;
    for (int c = '0'; c <= '9'; ++c) classify_handlers[c] = classify_m_digit;
    for (int c = 'a'; c <= 'z'; ++c) classify_handlers[c] = classify_m_alpha;
    for (int c = 'A'; c <= 'Z'; ++c) classify_handlers[c] = classify_m_alpha;
    classify_handlers['e'] = classify_handlers['E'] = classify_m_exponent;
    classify_handlers['"'] = classify_m_quote;
    classify_handlers[':'] = classify_m_colon;
    classify_handlers[','] = classify_m_comma;
    classify_handlers['{'] = classify_m_open_brace;
    classify_handlers['}'] = classify_m_close_brace;
    classify_handlers['['] = classify_m_open_bracket;
    classify_handlers[']'] = classify_m_close_bracket;
    classify_handlers['-'] = classify_m_minus;
    classify_handlers['+'] = classify_m_plus;
    classify_handlers['.'] = classify_m_dot;
    classify_handlers['\\'] = classify_m_backslash;
    classify_init_flag = true;
}

void classify_dispatch(ClassifyState& s) {
    ClassifyHandler h = classify_handlers[static_cast<unsigned char>(*s.cur)];
    MUSTTAIL return h(s);
}

[[gnu::noinline]]
uint64_t classify_musttail_batch(const char* data, size_t len) {
    init_classify_handlers();
    if (len == 0) return 0;
    ClassifyState s{data, data + len, 0};
    classify_dispatch(s);
    return s.sum;
}

} // namespace char_classify

// ============================================================================
// Part 2: JSON Tokenizer
// ============================================================================

namespace json_tokenizer {

// ============================================================================
// Baseline: Switch Tokenizer
// ============================================================================
class SwitchTokenizer {
public:
    explicit SwitchTokenizer(std::string_view input) : input_(input), pos_(0) {}
    
    JsonToken next_token() {
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++pos_; continue; }
            
            switch (c) {
            case '{': ++pos_; return JsonToken::ObjectStart;
            case '}': ++pos_; return JsonToken::ObjectEnd;
            case '[': ++pos_; return JsonToken::ArrayStart;
            case ']': ++pos_; return JsonToken::ArrayEnd;
            case ':': ++pos_; return JsonToken::Colon;
            case ',': ++pos_; return JsonToken::Comma;
            case '"': return parse_string();
            case 't': return parse_keyword("true", 4, JsonToken::True);
            case 'f': return parse_keyword("false", 5, JsonToken::False);
            case 'n': return parse_keyword("null", 4, JsonToken::Null);
            default:
                if ((c >= '0' && c <= '9') || c == '-') return parse_number();
                ++pos_;
                return JsonToken::Invalid;
            }
        }
        return JsonToken::End;
    }
    void reset() { pos_ = 0; }

private:
    JsonToken parse_string() {
        ++pos_;
        while (pos_ < input_.size()) {
            char c = input_[pos_++];
            if (c == '"') return JsonToken::String;
            if (c == '\\' && pos_ < input_.size()) ++pos_;
        }
        return JsonToken::Invalid;
    }
    JsonToken parse_number() {
        ++pos_;
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
                ++pos_;
            else break;
        }
        return JsonToken::Number;
    }
    JsonToken parse_keyword(const char* kw, size_t len, JsonToken token) {
        if (pos_ + len > input_.size()) return JsonToken::Invalid;
        for (size_t i = 0; i < len; ++i)
            if (input_[pos_ + i] != kw[i]) return JsonToken::Invalid;
        pos_ += len;
        return token;
    }
    
    std::string_view input_;
    size_t pos_;
};

// ============================================================================
// DirectThreaded Tokenizer (无 musttail) - 对照组
// ============================================================================
class DirectThreadedTokenizer {
public:
    explicit DirectThreadedTokenizer(std::string_view input) : input_(input), pos_(0) {
        init_handlers();
    }
    
    static void init_handlers() {
        if (initialized_) return;
        for (int i = 0; i < 256; ++i) handlers_[i] = &handle_invalid;
        handlers_[' '] = handlers_['\t'] = handlers_['\n'] = handlers_['\r'] = &handle_skip;
        handlers_['{'] = &handle_object_start;
        handlers_['}'] = &handle_object_end;
        handlers_['['] = &handle_array_start;
        handlers_[']'] = &handle_array_end;
        handlers_[':'] = &handle_colon;
        handlers_[','] = &handle_comma;
        handlers_['"'] = &handle_string;
        handlers_['t'] = &handle_true;
        handlers_['f'] = &handle_false;
        handlers_['n'] = &handle_null;
        handlers_['-'] = &handle_number;
        for (int c = '0'; c <= '9'; ++c) handlers_[c] = &handle_number;
        initialized_ = true;
    }
    
    JsonToken next_token() {
        while (pos_ < input_.size()) {
            unsigned char c = static_cast<unsigned char>(input_[pos_]);
            JsonToken result = handlers_[c](*this);
            if (result != JsonToken::Continue) return result;
        }
        return JsonToken::End;
    }
    void reset() { pos_ = 0; }

    std::string_view input_;
    size_t pos_;
    
private:
    using Handler = JsonToken(*)(DirectThreadedTokenizer&);
    
    static JsonToken handle_skip(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::Continue; }
    static JsonToken handle_object_start(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::ObjectStart; }
    static JsonToken handle_object_end(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::ObjectEnd; }
    static JsonToken handle_array_start(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::ArrayStart; }
    static JsonToken handle_array_end(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::ArrayEnd; }
    static JsonToken handle_colon(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::Colon; }
    static JsonToken handle_comma(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::Comma; }
    
    static JsonToken handle_string(DirectThreadedTokenizer& t) {
        ++t.pos_;
        while (t.pos_ < t.input_.size()) {
            char c = t.input_[t.pos_++];
            if (c == '"') return JsonToken::String;
            if (c == '\\' && t.pos_ < t.input_.size()) ++t.pos_;
        }
        return JsonToken::Invalid;
    }
    
    static JsonToken handle_number(DirectThreadedTokenizer& t) {
        ++t.pos_;
        while (t.pos_ < t.input_.size()) {
            char c = t.input_[t.pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
                ++t.pos_;
            else break;
        }
        return JsonToken::Number;
    }
    
    static JsonToken handle_true(DirectThreadedTokenizer& t) {
        if (t.pos_ + 4 <= t.input_.size() &&
            t.input_[t.pos_+1] == 'r' && t.input_[t.pos_+2] == 'u' && t.input_[t.pos_+3] == 'e') {
            t.pos_ += 4;
            return JsonToken::True;
        }
        ++t.pos_;
        return JsonToken::Invalid;
    }
    
    static JsonToken handle_false(DirectThreadedTokenizer& t) {
        if (t.pos_ + 5 <= t.input_.size() &&
            t.input_[t.pos_+1] == 'a' && t.input_[t.pos_+2] == 'l' &&
            t.input_[t.pos_+3] == 's' && t.input_[t.pos_+4] == 'e') {
            t.pos_ += 5;
            return JsonToken::False;
        }
        ++t.pos_;
        return JsonToken::Invalid;
    }
    
    static JsonToken handle_null(DirectThreadedTokenizer& t) {
        if (t.pos_ + 4 <= t.input_.size() &&
            t.input_[t.pos_+1] == 'u' && t.input_[t.pos_+2] == 'l' && t.input_[t.pos_+3] == 'l') {
            t.pos_ += 4;
            return JsonToken::Null;
        }
        ++t.pos_;
        return JsonToken::Invalid;
    }
    
    static JsonToken handle_invalid(DirectThreadedTokenizer& t) { ++t.pos_; return JsonToken::Invalid; }
    
    inline static bool initialized_ = false;
    inline static Handler handlers_[256];
};

// ============================================================================
// Musttail Tokenizer - 批处理整个输入
// ============================================================================

struct TokenizerState {
    const char* cur;
    const char* end;
    uint64_t token_sum;
    bool done;
};

using TokenHandler = void(*)(TokenizerState&);

// 终止 handler
void tok_done_handler(TokenizerState& s) {
    s.done = true;
}

// 前向声明
void tok_dispatch(TokenizerState& s);

// 跳过空白
void tok_skip(TokenizerState& s) {
    // 跳过所有连续空白
    while (s.cur < s.end && (*s.cur == ' ' || *s.cur == '\t' || *s.cur == '\n' || *s.cur == '\r'))
        ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

// 单字符 token
void tok_object_start(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::ObjectStart);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_object_end(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::ObjectEnd);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_array_start(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::ArrayStart);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_array_end(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::ArrayEnd);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_colon(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::Colon);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_comma(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::Comma);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

// String
void tok_string(TokenizerState& s) {
    ++s.cur; // skip opening "
    while (s.cur < s.end) {
        char c = *s.cur++;
        if (c == '"') {
            s.token_sum += static_cast<uint8_t>(JsonToken::String);
            TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
            MUSTTAIL return next(s);
        }
        if (c == '\\' && s.cur < s.end) ++s.cur;
    }
    s.done = true;  // Incomplete string
}

// Number
void tok_number(TokenizerState& s) {
    ++s.cur;
    while (s.cur < s.end) {
        char c = *s.cur;
        if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-')
            ++s.cur;
        else break;
    }
    s.token_sum += static_cast<uint8_t>(JsonToken::Number);
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

// Keywords
void tok_true(TokenizerState& s) {
    if (s.cur + 4 <= s.end && s.cur[1] == 'r' && s.cur[2] == 'u' && s.cur[3] == 'e') {
        s.cur += 4;
        s.token_sum += static_cast<uint8_t>(JsonToken::True);
        TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
        MUSTTAIL return next(s);
    }
    s.token_sum += static_cast<uint8_t>(JsonToken::Invalid);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_false(TokenizerState& s) {
    if (s.cur + 5 <= s.end && s.cur[1] == 'a' && s.cur[2] == 'l' && s.cur[3] == 's' && s.cur[4] == 'e') {
        s.cur += 5;
        s.token_sum += static_cast<uint8_t>(JsonToken::False);
        TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
        MUSTTAIL return next(s);
    }
    s.token_sum += static_cast<uint8_t>(JsonToken::Invalid);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_null(TokenizerState& s) {
    if (s.cur + 4 <= s.end && s.cur[1] == 'u' && s.cur[2] == 'l' && s.cur[3] == 'l') {
        s.cur += 4;
        s.token_sum += static_cast<uint8_t>(JsonToken::Null);
        TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
        MUSTTAIL return next(s);
    }
    s.token_sum += static_cast<uint8_t>(JsonToken::Invalid);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

void tok_invalid(TokenizerState& s) {
    s.token_sum += static_cast<uint8_t>(JsonToken::Invalid);
    ++s.cur;
    TokenHandler next = (s.cur < s.end) ? tok_dispatch : tok_done_handler;
    MUSTTAIL return next(s);
}

// Handler 表
inline TokenHandler tok_handlers[256];
inline bool tok_init_flag = false;

void init_tok_handlers() {
    if (tok_init_flag) return;
    for (int i = 0; i < 256; ++i) tok_handlers[i] = tok_invalid;
    tok_handlers[' '] = tok_handlers['\t'] = tok_handlers['\n'] = tok_handlers['\r'] = tok_skip;
    tok_handlers['{'] = tok_object_start;
    tok_handlers['}'] = tok_object_end;
    tok_handlers['['] = tok_array_start;
    tok_handlers[']'] = tok_array_end;
    tok_handlers[':'] = tok_colon;
    tok_handlers[','] = tok_comma;
    tok_handlers['"'] = tok_string;
    tok_handlers['t'] = tok_true;
    tok_handlers['f'] = tok_false;
    tok_handlers['n'] = tok_null;
    tok_handlers['-'] = tok_number;
    for (int c = '0'; c <= '9'; ++c) tok_handlers[c] = tok_number;
    tok_init_flag = true;
}

void tok_dispatch(TokenizerState& s) {
    // 先跳过空白
    while (s.cur < s.end && (*s.cur == ' ' || *s.cur == '\t' || *s.cur == '\n' || *s.cur == '\r'))
        ++s.cur;
    
    TokenHandler h = (s.cur < s.end) ? tok_handlers[static_cast<unsigned char>(*s.cur)] : tok_done_handler;
    MUSTTAIL return h(s);
}

[[gnu::noinline]]
uint64_t tokenize_musttail(std::string_view input) {
    init_tok_handlers();
    if (input.empty()) return 0;
    TokenizerState s{input.data(), input.data() + input.size(), 0, false};
    tok_dispatch(s);
    return s.token_sum;
}

} // namespace json_tokenizer

// ============================================================================
// Part 3: VM Interpreter
// ============================================================================

namespace json_vm {

enum class OpCode : uint8_t {
    Nop = 0, SkipWhitespace, MatchChar, MatchRange,
    JumpIfMatch, JumpIfNoMatch, Jump, EmitToken, Halt, Count
};

struct Instruction {
    OpCode op;
    uint8_t operand1;
    uint8_t operand2;
    uint8_t operand3;
};

// ============================================================================
// Switch VM (baseline)
// ============================================================================
class SwitchVM {
public:
    SwitchVM(std::span<const Instruction> prog, std::string_view input)
        : prog_(prog), input_(input) {}
    
    JsonToken run() {
        size_t pc = 0, pos = 0;
        bool match = false;
        
        while (pc < prog_.size()) {
            const auto& inst = prog_[pc];
            switch (inst.op) {
            case OpCode::Nop: ++pc; break;
            case OpCode::SkipWhitespace:
                while (pos < input_.size() && 
                       (input_[pos] == ' ' || input_[pos] == '\t' ||
                        input_[pos] == '\n' || input_[pos] == '\r')) ++pos;
                ++pc;
                break;
            case OpCode::MatchChar:
                match = (pos < input_.size() && input_[pos] == static_cast<char>(inst.operand1));
                if (match) ++pos;
                ++pc;
                break;
            case OpCode::MatchRange:
                match = (pos < input_.size() && 
                         input_[pos] >= static_cast<char>(inst.operand1) &&
                         input_[pos] <= static_cast<char>(inst.operand2));
                if (match) ++pos;
                ++pc;
                break;
            case OpCode::JumpIfMatch:
                pc = match ? inst.operand1 : pc + 1;
                break;
            case OpCode::JumpIfNoMatch:
                pc = match ? pc + 1 : inst.operand1;
                break;
            case OpCode::Jump:
                pc = inst.operand1;
                break;
            case OpCode::EmitToken:
                return static_cast<JsonToken>(inst.operand1);
            case OpCode::Halt:
                return JsonToken::End;
            default:
                return JsonToken::Invalid;
            }
        }
        return JsonToken::End;
    }

private:
    std::span<const Instruction> prog_;
    std::string_view input_;
};

// ============================================================================
// Computed Goto VM
// ============================================================================
#if defined(__GNUC__) || defined(__clang__)
class ComputedGotoVM {
public:
    ComputedGotoVM(std::span<const Instruction> prog, std::string_view input)
        : prog_(prog), input_(input) {}
    
    JsonToken run() {
        static void* dispatch[] = {
            &&L_nop, &&L_skip_ws, &&L_match_char, &&L_match_range,
            &&L_jmp_match, &&L_jmp_no_match, &&L_jmp, &&L_emit, &&L_halt
        };
        
        size_t pc = 0, pos = 0;
        bool match = false;
        
        #define DISPATCH() goto *dispatch[static_cast<int>(prog_[pc].op)]
        
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
        
    L_nop: ++pc; if (pc >= prog_.size()) return JsonToken::End; DISPATCH();
    L_skip_ws:
        while (pos < input_.size() &&
               (input_[pos] == ' ' || input_[pos] == '\t' ||
                input_[pos] == '\n' || input_[pos] == '\r')) ++pos;
        ++pc;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_match_char:
        match = (pos < input_.size() && input_[pos] == static_cast<char>(prog_[pc].operand1));
        if (match) ++pos;
        ++pc;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_match_range:
        match = (pos < input_.size() &&
                input_[pos] >= static_cast<char>(prog_[pc].operand1) &&
                input_[pos] <= static_cast<char>(prog_[pc].operand2));
        if (match) ++pos;
        ++pc;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_jmp_match:
        pc = match ? prog_[pc].operand1 : pc + 1;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_jmp_no_match:
        pc = match ? pc + 1 : prog_[pc].operand1;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_jmp:
        pc = prog_[pc].operand1;
        if (pc >= prog_.size()) return JsonToken::End;
        DISPATCH();
    L_emit:
        return static_cast<JsonToken>(prog_[pc].operand1);
    L_halt:
        return JsonToken::End;
        
        #undef DISPATCH
    }

private:
    std::span<const Instruction> prog_;
    std::string_view input_;
};
#endif

// ============================================================================
// Musttail VM
// ============================================================================
struct VMState {
    const Instruction* prog;
    size_t prog_size;
    const char* input;
    size_t input_size;
    size_t pc;
    size_t pos;
    bool match;
    JsonToken result;
    bool done;
};

using VMHandler = void(*)(VMState&);

void vm_done(VMState& s) { s.done = true; }
void vm_dispatch(VMState& s);

void vm_nop(VMState& s) {
    ++s.pc;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_skip_ws(VMState& s) {
    while (s.pos < s.input_size &&
           (s.input[s.pos] == ' ' || s.input[s.pos] == '\t' ||
            s.input[s.pos] == '\n' || s.input[s.pos] == '\r')) ++s.pos;
    ++s.pc;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_match_char(VMState& s) {
    s.match = (s.pos < s.input_size && s.input[s.pos] == static_cast<char>(s.prog[s.pc].operand1));
    if (s.match) ++s.pos;
    ++s.pc;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_match_range(VMState& s) {
    s.match = (s.pos < s.input_size &&
               s.input[s.pos] >= static_cast<char>(s.prog[s.pc].operand1) &&
               s.input[s.pos] <= static_cast<char>(s.prog[s.pc].operand2));
    if (s.match) ++s.pos;
    ++s.pc;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_jmp_match(VMState& s) {
    s.pc = s.match ? s.prog[s.pc].operand1 : s.pc + 1;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_jmp_no_match(VMState& s) {
    s.pc = s.match ? s.pc + 1 : s.prog[s.pc].operand1;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_jmp(VMState& s) {
    s.pc = s.prog[s.pc].operand1;
    VMHandler next = (s.pc < s.prog_size) ? vm_dispatch : vm_done;
    MUSTTAIL return next(s);
}

void vm_emit(VMState& s) {
    s.result = static_cast<JsonToken>(s.prog[s.pc].operand1);
    s.done = true;
}

void vm_halt(VMState& s) {
    s.result = JsonToken::End;
    s.done = true;
}

inline VMHandler vm_handlers[static_cast<int>(OpCode::Count)];
inline bool vm_init_flag = false;

void init_vm_handlers() {
    if (vm_init_flag) return;
    vm_handlers[0] = vm_nop;
    vm_handlers[1] = vm_skip_ws;
    vm_handlers[2] = vm_match_char;
    vm_handlers[3] = vm_match_range;
    vm_handlers[4] = vm_jmp_match;
    vm_handlers[5] = vm_jmp_no_match;
    vm_handlers[6] = vm_jmp;
    vm_handlers[7] = vm_emit;
    vm_handlers[8] = vm_halt;
    vm_init_flag = true;
}

void vm_dispatch(VMState& s) {
    VMHandler h = vm_handlers[static_cast<int>(s.prog[s.pc].op)];
    MUSTTAIL return h(s);
}

[[gnu::noinline]]
JsonToken run_musttail_vm(std::span<const Instruction> prog, std::string_view input) {
    init_vm_handlers();
    if (prog.empty()) return JsonToken::End;
    
    VMState s{
        prog.data(), prog.size(),
        input.data(), input.size(),
        0, 0, false, JsonToken::Invalid, false
    };
    vm_dispatch(s);
    return s.result;
}

inline std::vector<Instruction> make_number_program() {
    return {
        {OpCode::SkipWhitespace, 0, 0, 0},
        {OpCode::MatchChar, '-', 0, 0},
        {OpCode::MatchRange, '0', '9', 0},
        {OpCode::JumpIfNoMatch, 8, 0, 0},
        {OpCode::MatchRange, '0', '9', 0},
        {OpCode::JumpIfMatch, 4, 0, 0},
        {OpCode::EmitToken, static_cast<uint8_t>(JsonToken::Number), 0, 0},
        {OpCode::Halt, 0, 0, 0},
        {OpCode::EmitToken, static_cast<uint8_t>(JsonToken::Invalid), 0, 0},
    };
}

} // namespace json_vm

// ============================================================================
// Test Data
// ============================================================================

namespace test_data {

std::string generate_json(size_t size_hint) {
    std::mt19937 rng(42);
    std::string result;
    result.reserve(size_hint);
    
    auto random_string = [&](size_t len) {
        std::string s = "\"";
        for (size_t i = 0; i < len; ++i) s += 'a' + (rng() % 26);
        s += '"';
        return s;
    };
    
    auto random_number = [&]() {
        std::string s;
        if (rng() % 4 == 0) s += '-';
        for (size_t i = 0; i < 1 + (rng() % 8); ++i) s += '0' + (rng() % 10);
        if (rng() % 3 == 0) {
            s += '.';
            for (size_t i = 0; i < 1 + (rng() % 4); ++i) s += '0' + (rng() % 10);
        }
        return s;
    };
    
    result += "[\n";
    while (result.size() < size_hint - 100) {
        result += "  {";
        for (size_t i = 0; i < 1 + (rng() % 5); ++i) {
            if (i > 0) result += ", ";
            result += random_string(4 + (rng() % 8)) + ": ";
            switch (rng() % 5) {
            case 0: result += random_string(5 + (rng() % 20)); break;
            case 1: result += random_number(); break;
            case 2: result += "true"; break;
            case 3: result += "false"; break;
            case 4: result += "null"; break;
            }
        }
        result += "},\n";
    }
    if (result.size() > 2 && result[result.size()-2] == ',') {
        result[result.size()-2] = '\n';
        result.pop_back();
    }
    result += "]";
    return result;
}

std::string generate_random_chars(size_t size) {
    std::mt19937 rng(42);
    std::string result;
    result.reserve(size);
    static const char chars[] = "{}[]\":,0123456789abcdefghijklmnopqrstuvwxyz \t\n-+.\\";
    for (size_t i = 0; i < size; ++i) result += chars[rng() % (sizeof(chars) - 1)];
    return result;
}

inline std::string small_json, medium_json, large_json, random_chars;

void init() {
    if (!small_json.empty()) return;
    small_json = generate_json(1024);
    medium_json = generate_json(64 * 1024);
    large_json = generate_json(1024 * 1024);
    random_chars = generate_random_chars(1024 * 1024);
    std::cout << "Test data: small=" << small_json.size() << "B, medium=" 
              << medium_json.size() << "B, large=" << large_json.size() << "B\n";
}

} // namespace test_data

// ============================================================================
// Tests
// ============================================================================

namespace tests {

void run_all_tests() {
    std::cout << "Running tests...\n";
    
    // Test 1: Character classification
    {
        using namespace char_classify;
        init_classify_handlers();
        
        bool ok = true;
        ok &= (classify_table(' ') == CharClass::Whitespace);
        ok &= (classify_table('0') == CharClass::Digit);
        ok &= (classify_table('{') == CharClass::OpenBrace);
        
        // Test musttail batch
        const char* test = "{}[]0123abc";
        uint64_t sum = classify_musttail_batch(test, strlen(test));
        
        // Verify: each char should contribute to sum
        uint64_t expected = 0;
        for (const char* p = test; *p; ++p) {
            expected += static_cast<uint8_t>(classify_table(*p));
        }
        ok &= (sum == expected);
        
        std::cout << "  CharClassify: " << (ok ? "PASSED" : "FAILED") 
                  << " (sum=" << sum << ", expected=" << expected << ")\n";
    }
    
    // Test 2: Tokenizer
    {
        using namespace json_tokenizer;
        init_tok_handlers();
        DirectThreadedTokenizer::init_handlers();
        
        std::string_view json = R"({"a": 123, "b": true})";
        // Tokens: { "a" : 123 , "b" : true } = 9 tokens
        
        // Switch tokenizer
        SwitchTokenizer st(json);
        std::vector<JsonToken> tokens1;
        JsonToken t;
        while ((t = st.next_token()) != JsonToken::End) {
            tokens1.push_back(t);
        }
        
        // Compute expected sum
        uint64_t expected_sum = 0;
        for (auto tok : tokens1) {
            expected_sum += static_cast<uint8_t>(tok);
        }
        
        // Musttail tokenizer
        uint64_t musttail_sum = tokenize_musttail(json);
        
        bool ok = (tokens1.size() == 9) && (musttail_sum == expected_sum);
        
        if (!ok) {
            std::cout << "    Switch tokens: " << tokens1.size() << "\n";
            std::cout << "    Expected sum: " << expected_sum << ", Musttail sum: " << musttail_sum << "\n";
            std::cout << "    Tokens: ";
            for (auto tok : tokens1) std::cout << token_name(tok) << " ";
            std::cout << "\n";
        }
        
        std::cout << "  Tokenizer: " << (ok ? "PASSED" : "FAILED") << "\n";
    }
    
    // Test 3: VM
    {
        using namespace json_vm;
        init_vm_handlers();
        
        auto prog = make_number_program();
        std::string_view input = "  -12345";
        
        bool ok = true;
        ok &= (SwitchVM(prog, input).run() == JsonToken::Number);
        ok &= (ComputedGotoVM(prog, input).run() == JsonToken::Number);
        ok &= (run_musttail_vm(prog, input) == JsonToken::Number);
        
        std::cout << "  VM: " << (ok ? "PASSED" : "FAILED") << "\n";
    }
    
    std::cout << "Tests completed.\n\n";
}

} // namespace tests

// ============================================================================
// Benchmarks
// ============================================================================

// Character Classification
static void BM_CharClassify_Table(benchmark::State& state) {
    test_data::init();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_table(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Table);

static void BM_CharClassify_Direct(benchmark::State& state) {
    test_data::init();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_direct(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Direct);

static void BM_CharClassify_Musttail(benchmark::State& state) {
    test_data::init();
    char_classify::init_classify_handlers();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = char_classify::classify_musttail_batch(data.data(), data.size());
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Musttail);

// Tokenizer
template<typename T>
static void tokenize_all(T& tok, uint64_t& sum) {
    JsonToken t;
    while ((t = tok.next_token()) != JsonToken::End) sum += static_cast<uint8_t>(t);
}

#define TOKENIZER_BENCH(Name, DataField) \
static void BM_Tokenizer_Switch_##Name(benchmark::State& state) { \
    test_data::init(); \
    const auto& data = test_data::DataField; \
    for (auto _ : state) { \
        json_tokenizer::SwitchTokenizer tok(data); \
        uint64_t sum = 0; \
        tokenize_all(tok, sum); \
        benchmark::DoNotOptimize(sum); \
    } \
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size())); \
} \
BENCHMARK(BM_Tokenizer_Switch_##Name); \
\
static void BM_Tokenizer_Direct_##Name(benchmark::State& state) { \
    test_data::init(); \
    json_tokenizer::DirectThreadedTokenizer::init_handlers(); \
    const auto& data = test_data::DataField; \
    for (auto _ : state) { \
        json_tokenizer::DirectThreadedTokenizer tok(data); \
        uint64_t sum = 0; \
        tokenize_all(tok, sum); \
        benchmark::DoNotOptimize(sum); \
    } \
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size())); \
} \
BENCHMARK(BM_Tokenizer_Direct_##Name); \
\
static void BM_Tokenizer_Musttail_##Name(benchmark::State& state) { \
    test_data::init(); \
    json_tokenizer::init_tok_handlers(); \
    const auto& data = test_data::DataField; \
    for (auto _ : state) { \
        uint64_t sum = json_tokenizer::tokenize_musttail(data); \
        benchmark::DoNotOptimize(sum); \
    } \
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size())); \
} \
BENCHMARK(BM_Tokenizer_Musttail_##Name);

TOKENIZER_BENCH(Small, small_json)
TOKENIZER_BENCH(Medium, medium_json)
TOKENIZER_BENCH(Large, large_json)

// VM Benchmarks
static void BM_VM_Switch(benchmark::State& state) {
    auto prog = json_vm::make_number_program();
    std::string input = "  -12345678901234567890";
    for (auto _ : state) {
        auto r = json_vm::SwitchVM(prog, input).run();
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_VM_Switch);

static void BM_VM_ComputedGoto(benchmark::State& state) {
    auto prog = json_vm::make_number_program();
    std::string input = "  -12345678901234567890";
    for (auto _ : state) {
        auto r = json_vm::ComputedGotoVM(prog, input).run();
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_VM_ComputedGoto);

static void BM_VM_Musttail(benchmark::State& state) {
    json_vm::init_vm_handlers();
    auto prog = json_vm::make_number_program();
    std::string input = "  -12345678901234567890";
    for (auto _ : state) {
        auto r = json_vm::run_musttail_vm(prog, input);
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_VM_Musttail);

int main(int argc, char** argv) {
    tests::run_all_tests();
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}