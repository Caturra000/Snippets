// json_threaded_benchmark.cpp
// 编译: g++ -std=c++23 -O3 -march=native -o bench json_threaded_benchmark.cpp -lbenchmark -lpthread

#include <benchmark/benchmark.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <span>
#include <iostream>

// ============================================================================
// Part 0: Common Types and Utilities
// ============================================================================

enum class JsonToken : uint8_t {
    Invalid = 0,
    ObjectStart,   // {
    ObjectEnd,     // }
    ArrayStart,    // [
    ArrayEnd,      // ]
    String,        // "..."
    Number,        // 123, -45.67e+8
    True,          // true
    False,         // false
    Null,          // null
    Colon,         // :
    Comma,         // ,
    Whitespace,    // space, tab, newline
    End,           // end of input
    Count
};

const char* token_name(JsonToken t) {
    switch (t) {
        case JsonToken::Invalid: return "Invalid";
        case JsonToken::ObjectStart: return "ObjectStart";
        case JsonToken::ObjectEnd: return "ObjectEnd";
        case JsonToken::ArrayStart: return "ArrayStart";
        case JsonToken::ArrayEnd: return "ArrayEnd";
        case JsonToken::String: return "String";
        case JsonToken::Number: return "Number";
        case JsonToken::True: return "True";
        case JsonToken::False: return "False";
        case JsonToken::Null: return "Null";
        case JsonToken::Colon: return "Colon";
        case JsonToken::Comma: return "Comma";
        case JsonToken::End: return "End";
        default: return "Unknown";
    }
}

enum class CharClass : uint8_t {
    Invalid = 0,
    Whitespace,    // space, tab, \n, \r
    Digit,         // 0-9
    Alpha,         // a-z, A-Z (except e, E)
    Quote,         // "
    Colon,         // :
    Comma,         // ,
    OpenBrace,     // {
    CloseBrace,    // }
    OpenBracket,   // [
    CloseBracket,  // ]
    Minus,         // -
    Plus,          // +
    Dot,           // .
    Backslash,     // '\'
    Exponent,      // e, E
    Other,         // anything else
    Count
};

// ============================================================================
// Part 1: Character Classification - 5 Methods
// ============================================================================

namespace char_classify {

// ----------------------------------------------------------------------------
// Method 1: Traditional Switch-Case
// ----------------------------------------------------------------------------
[[gnu::noinline]]
CharClass classify_switch(char c) {
    switch (c) {
        case ' ': case '\t': case '\n': case '\r':
            return CharClass::Whitespace;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return CharClass::Digit;
        case 'a': case 'b': case 'c': case 'd': case 'f': case 'g':
        case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
        case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
            return CharClass::Alpha;
        case 'e': case 'E':
            return CharClass::Exponent;
        case '"':
            return CharClass::Quote;
        case ':':
            return CharClass::Colon;
        case ',':
            return CharClass::Comma;
        case '{':
            return CharClass::OpenBrace;
        case '}':
            return CharClass::CloseBrace;
        case '[':
            return CharClass::OpenBracket;
        case ']':
            return CharClass::CloseBracket;
        case '-':
            return CharClass::Minus;
        case '+':
            return CharClass::Plus;
        case '.':
            return CharClass::Dot;
        case '\\':
            return CharClass::Backslash;
        default:
            return CharClass::Other;
    }
}

// ----------------------------------------------------------------------------
// Method 2: Lookup Table (Direct Threaded Data)
// ----------------------------------------------------------------------------
constexpr std::array<CharClass, 256> make_char_table() {
    std::array<CharClass, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = CharClass::Other;
    }
    table[static_cast<unsigned char>(' ')] = CharClass::Whitespace;
    table[static_cast<unsigned char>('\t')] = CharClass::Whitespace;
    table[static_cast<unsigned char>('\n')] = CharClass::Whitespace;
    table[static_cast<unsigned char>('\r')] = CharClass::Whitespace;
    for (char c = '0'; c <= '9'; ++c) {
        table[static_cast<unsigned char>(c)] = CharClass::Digit;
    }
    for (char c = 'a'; c <= 'z'; ++c) {
        table[static_cast<unsigned char>(c)] = CharClass::Alpha;
    }
    for (char c = 'A'; c <= 'Z'; ++c) {
        table[static_cast<unsigned char>(c)] = CharClass::Alpha;
    }
    table[static_cast<unsigned char>('e')] = CharClass::Exponent;
    table[static_cast<unsigned char>('E')] = CharClass::Exponent;
    table[static_cast<unsigned char>('"')] = CharClass::Quote;
    table[static_cast<unsigned char>(':')] = CharClass::Colon;
    table[static_cast<unsigned char>(',')] = CharClass::Comma;
    table[static_cast<unsigned char>('{')] = CharClass::OpenBrace;
    table[static_cast<unsigned char>('}')] = CharClass::CloseBrace;
    table[static_cast<unsigned char>('[')] = CharClass::OpenBracket;
    table[static_cast<unsigned char>(']')] = CharClass::CloseBracket;
    table[static_cast<unsigned char>('-')] = CharClass::Minus;
    table[static_cast<unsigned char>('+')] = CharClass::Plus;
    table[static_cast<unsigned char>('.')] = CharClass::Dot;
    table[static_cast<unsigned char>('\\')] = CharClass::Backslash;
    return table;
}

inline constexpr auto kCharTable = make_char_table();

[[gnu::noinline]]
CharClass classify_table(char c) {
    return kCharTable[static_cast<unsigned char>(c)];
}

// ----------------------------------------------------------------------------
// Method 3: Direct Threaded Code (Function Pointer Table)
// ----------------------------------------------------------------------------
using ClassifyFunc = CharClass(*)(char);

CharClass handle_whitespace(char) { return CharClass::Whitespace; }
CharClass handle_digit(char) { return CharClass::Digit; }
CharClass handle_alpha(char) { return CharClass::Alpha; }
CharClass handle_exponent(char) { return CharClass::Exponent; }
CharClass handle_quote(char) { return CharClass::Quote; }
CharClass handle_colon(char) { return CharClass::Colon; }
CharClass handle_comma(char) { return CharClass::Comma; }
CharClass handle_open_brace(char) { return CharClass::OpenBrace; }
CharClass handle_close_brace(char) { return CharClass::CloseBrace; }
CharClass handle_open_bracket(char) { return CharClass::OpenBracket; }
CharClass handle_close_bracket(char) { return CharClass::CloseBracket; }
CharClass handle_minus(char) { return CharClass::Minus; }
CharClass handle_plus(char) { return CharClass::Plus; }
CharClass handle_dot(char) { return CharClass::Dot; }
CharClass handle_backslash(char) { return CharClass::Backslash; }
CharClass handle_other(char) { return CharClass::Other; }

constexpr std::array<ClassifyFunc, 256> make_func_table() {
    std::array<ClassifyFunc, 256> table{};
    for (int i = 0; i < 256; ++i) {
        table[i] = handle_other;
    }
    table[static_cast<unsigned char>(' ')] = handle_whitespace;
    table[static_cast<unsigned char>('\t')] = handle_whitespace;
    table[static_cast<unsigned char>('\n')] = handle_whitespace;
    table[static_cast<unsigned char>('\r')] = handle_whitespace;
    for (char c = '0'; c <= '9'; ++c) {
        table[static_cast<unsigned char>(c)] = handle_digit;
    }
    for (char c = 'a'; c <= 'z'; ++c) {
        table[static_cast<unsigned char>(c)] = handle_alpha;
    }
    for (char c = 'A'; c <= 'Z'; ++c) {
        table[static_cast<unsigned char>(c)] = handle_alpha;
    }
    table[static_cast<unsigned char>('e')] = handle_exponent;
    table[static_cast<unsigned char>('E')] = handle_exponent;
    table[static_cast<unsigned char>('"')] = handle_quote;
    table[static_cast<unsigned char>(':')] = handle_colon;
    table[static_cast<unsigned char>(',')] = handle_comma;
    table[static_cast<unsigned char>('{')] = handle_open_brace;
    table[static_cast<unsigned char>('}')] = handle_close_brace;
    table[static_cast<unsigned char>('[')] = handle_open_bracket;
    table[static_cast<unsigned char>(']')] = handle_close_bracket;
    table[static_cast<unsigned char>('-')] = handle_minus;
    table[static_cast<unsigned char>('+')] = handle_plus;
    table[static_cast<unsigned char>('.')] = handle_dot;
    table[static_cast<unsigned char>('\\')] = handle_backslash;
    return table;
}

inline constexpr auto kFuncTable = make_func_table();

[[gnu::noinline]]
CharClass classify_direct_threaded(char c) {
    return kFuncTable[static_cast<unsigned char>(c)](c);
}

// ----------------------------------------------------------------------------
// Method 4: Computed Goto (GCC/Clang Extension)
// ----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
CharClass classify_computed_goto(char c) {
    static void* labels[256] = {nullptr};
    static bool initialized = false;
    
    if (!initialized) [[unlikely]] {
        for (int i = 0; i < 256; ++i) {
            labels[i] = &&L_other;
        }
        labels[static_cast<unsigned char>(' ')] = &&L_whitespace;
        labels[static_cast<unsigned char>('\t')] = &&L_whitespace;
        labels[static_cast<unsigned char>('\n')] = &&L_whitespace;
        labels[static_cast<unsigned char>('\r')] = &&L_whitespace;
        
        for (int i = '0'; i <= '9'; ++i) labels[i] = &&L_digit;
        for (int i = 'a'; i <= 'z'; ++i) labels[i] = &&L_alpha;
        for (int i = 'A'; i <= 'Z'; ++i) labels[i] = &&L_alpha;
        
        labels[static_cast<unsigned char>('e')] = &&L_exponent;
        labels[static_cast<unsigned char>('E')] = &&L_exponent;
        labels[static_cast<unsigned char>('"')] = &&L_quote;
        labels[static_cast<unsigned char>(':')] = &&L_colon;
        labels[static_cast<unsigned char>(',')] = &&L_comma;
        labels[static_cast<unsigned char>('{')] = &&L_open_brace;
        labels[static_cast<unsigned char>('}')] = &&L_close_brace;
        labels[static_cast<unsigned char>('[')] = &&L_open_bracket;
        labels[static_cast<unsigned char>(']')] = &&L_close_bracket;
        labels[static_cast<unsigned char>('-')] = &&L_minus;
        labels[static_cast<unsigned char>('+')] = &&L_plus;
        labels[static_cast<unsigned char>('.')] = &&L_dot;
        labels[static_cast<unsigned char>('\\')] = &&L_backslash;
        
        initialized = true;
    }
    
    goto *labels[static_cast<unsigned char>(c)];
    
    L_whitespace:    return CharClass::Whitespace;
    L_digit:         return CharClass::Digit;
    L_alpha:         return CharClass::Alpha;
    L_exponent:      return CharClass::Exponent;
    L_quote:         return CharClass::Quote;
    L_colon:         return CharClass::Colon;
    L_comma:         return CharClass::Comma;
    L_open_brace:    return CharClass::OpenBrace;
    L_close_brace:   return CharClass::CloseBrace;
    L_open_bracket:  return CharClass::OpenBracket;
    L_close_bracket: return CharClass::CloseBracket;
    L_minus:         return CharClass::Minus;
    L_plus:          return CharClass::Plus;
    L_dot:           return CharClass::Dot;
    L_backslash:     return CharClass::Backslash;
    L_other:         return CharClass::Other;
}
#else
CharClass classify_computed_goto(char c) { return classify_table(c); }
#endif

// ----------------------------------------------------------------------------
// Method 5: Hybrid (Range Check + Table)
// ----------------------------------------------------------------------------
[[gnu::noinline]]
CharClass classify_hybrid(char c) {
    unsigned char uc = static_cast<unsigned char>(c);
    
    if (uc <= ' ') [[likely]] {
        if (c == ' ' || c == '\n' || c == '\t' || c == '\r')
            return CharClass::Whitespace;
        return CharClass::Other;
    }
    
    if (uc >= '0' && uc <= '9') [[likely]] {
        return CharClass::Digit;
    }
    
    if ((uc >= 'a' && uc <= 'z') || (uc >= 'A' && uc <= 'Z')) {
        if (c == 'e' || c == 'E') return CharClass::Exponent;
        return CharClass::Alpha;
    }
    
    return kCharTable[uc];
}

} // namespace char_classify

// ============================================================================
// Part 2: JSON Tokenizer - State Machine Implementation
// ============================================================================

namespace json_tokenizer {

// ----------------------------------------------------------------------------
// Method 1: Switch-Case State Machine
// ----------------------------------------------------------------------------
class SwitchTokenizer {
public:
    explicit SwitchTokenizer(std::string_view input) 
        : input_(input), pos_(0) {}
    
    JsonToken next_token() {
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            
            // Skip whitespace
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
                continue;
            }
            
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
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return parse_number();
            default:
                ++pos_;
                return JsonToken::Invalid;
            }
        }
        return JsonToken::End;
    }
    
    void reset() { pos_ = 0; }

private:
    JsonToken parse_string() {
        ++pos_;  // skip opening "
        while (pos_ < input_.size()) {
            char c = input_[pos_++];
            if (c == '"') return JsonToken::String;
            if (c == '\\' && pos_ < input_.size()) ++pos_;
        }
        return JsonToken::Invalid;
    }
    
    JsonToken parse_number() {
        ++pos_;  // consume first char
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || 
                c == '+' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        return JsonToken::Number;
    }
    
    JsonToken parse_keyword(const char* kw, size_t len, JsonToken token) {
        if (pos_ + len > input_.size()) return JsonToken::Invalid;
        for (size_t i = 0; i < len; ++i) {
            if (input_[pos_ + i] != kw[i]) return JsonToken::Invalid;
        }
        pos_ += len;
        return token;
    }
    
    std::string_view input_;
    size_t pos_;
};

// ----------------------------------------------------------------------------
// Method 2: Table-Driven State Machine (Hybrid for keywords)
// ----------------------------------------------------------------------------
class TableTokenizer {
public:
    explicit TableTokenizer(std::string_view input)
        : input_(input), pos_(0) {
        init_tables();
    }
    
    static void init_tables() {
        if (initialized_) return;
        
        // Character class table
        for (int i = 0; i < 256; ++i) char_class_[i] = CC_OTHER;
        
        char_class_[static_cast<unsigned char>(' ')] = CC_WS;
        char_class_[static_cast<unsigned char>('\t')] = CC_WS;
        char_class_[static_cast<unsigned char>('\n')] = CC_WS;
        char_class_[static_cast<unsigned char>('\r')] = CC_WS;
        char_class_[static_cast<unsigned char>('{')] = CC_LBRACE;
        char_class_[static_cast<unsigned char>('}')] = CC_RBRACE;
        char_class_[static_cast<unsigned char>('[')] = CC_LBRACKET;
        char_class_[static_cast<unsigned char>(']')] = CC_RBRACKET;
        char_class_[static_cast<unsigned char>('"')] = CC_QUOTE;
        char_class_[static_cast<unsigned char>(':')] = CC_COLON;
        char_class_[static_cast<unsigned char>(',')] = CC_COMMA;
        char_class_[static_cast<unsigned char>('-')] = CC_MINUS;
        for (int c = '0'; c <= '9'; ++c) char_class_[c] = CC_DIGIT;
        char_class_[static_cast<unsigned char>('.')] = CC_DOT;
        char_class_[static_cast<unsigned char>('e')] = CC_EXP;
        char_class_[static_cast<unsigned char>('E')] = CC_EXP;
        char_class_[static_cast<unsigned char>('+')] = CC_PLUS;
        char_class_[static_cast<unsigned char>('t')] = CC_T;
        char_class_[static_cast<unsigned char>('f')] = CC_F;
        char_class_[static_cast<unsigned char>('n')] = CC_N;
        char_class_[static_cast<unsigned char>('\\')] = CC_BACKSLASH;
        
        // Token table for single-char tokens from Start state
        for (int i = 0; i < CC_COUNT; ++i) token_table_[i] = JsonToken::Invalid;
        token_table_[CC_LBRACE] = JsonToken::ObjectStart;
        token_table_[CC_RBRACE] = JsonToken::ObjectEnd;
        token_table_[CC_LBRACKET] = JsonToken::ArrayStart;
        token_table_[CC_RBRACKET] = JsonToken::ArrayEnd;
        token_table_[CC_COLON] = JsonToken::Colon;
        token_table_[CC_COMMA] = JsonToken::Comma;
        
        initialized_ = true;
    }
    
    JsonToken next_token() {
        // Skip whitespace
        while (pos_ < input_.size() && char_class_[static_cast<unsigned char>(input_[pos_])] == CC_WS) {
            ++pos_;
        }
        
        if (pos_ >= input_.size()) return JsonToken::End;
        
        uint8_t cc = char_class_[static_cast<unsigned char>(input_[pos_])];
        
        // Single-char tokens via table lookup
        if (token_table_[cc] != JsonToken::Invalid) {
            ++pos_;
            return token_table_[cc];
        }
        
        // Multi-char tokens
        switch (cc) {
        case CC_QUOTE: return parse_string();
        case CC_MINUS:
        case CC_DIGIT: return parse_number();
        case CC_T: return parse_keyword("true", 4, JsonToken::True);
        case CC_F: return parse_keyword("false", 5, JsonToken::False);
        case CC_N: return parse_keyword("null", 4, JsonToken::Null);
        default:
            ++pos_;
            return JsonToken::Invalid;
        }
    }
    
    void reset() { pos_ = 0; }

private:
    enum CharClassId : uint8_t {
        CC_WS = 0, CC_LBRACE, CC_RBRACE, CC_LBRACKET, CC_RBRACKET,
        CC_QUOTE, CC_COLON, CC_COMMA, CC_MINUS, CC_DIGIT, CC_DOT,
        CC_EXP, CC_PLUS, CC_T, CC_F, CC_N, CC_BACKSLASH, CC_OTHER, CC_COUNT
    };
    
    JsonToken parse_string() {
        ++pos_;
        while (pos_ < input_.size()) {
            uint8_t cc = char_class_[static_cast<unsigned char>(input_[pos_++])];
            if (cc == CC_QUOTE) return JsonToken::String;
            if (cc == CC_BACKSLASH && pos_ < input_.size()) ++pos_;
        }
        return JsonToken::Invalid;
    }
    
    JsonToken parse_number() {
        ++pos_;
        while (pos_ < input_.size()) {
            uint8_t cc = char_class_[static_cast<unsigned char>(input_[pos_])];
            if (cc == CC_DIGIT || cc == CC_DOT || cc == CC_EXP || 
                cc == CC_PLUS || cc == CC_MINUS) {
                ++pos_;
            } else {
                break;
            }
        }
        return JsonToken::Number;
    }
    
    JsonToken parse_keyword(const char* kw, size_t len, JsonToken token) {
        if (pos_ + len > input_.size()) return JsonToken::Invalid;
        for (size_t i = 0; i < len; ++i) {
            if (input_[pos_ + i] != kw[i]) return JsonToken::Invalid;
        }
        pos_ += len;
        return token;
    }
    
    std::string_view input_;
    size_t pos_;
    
    inline static bool initialized_ = false;
    inline static uint8_t char_class_[256];
    inline static JsonToken token_table_[CC_COUNT];
};

// ----------------------------------------------------------------------------
// Method 3: Direct Threaded Code (Function Pointer per Token Type)
// ----------------------------------------------------------------------------
class DirectThreadedTokenizer;
using TokenHandler = JsonToken(*)(DirectThreadedTokenizer&);

class DirectThreadedTokenizer {
public:
    explicit DirectThreadedTokenizer(std::string_view input)
        : input_(input), pos_(0) {
        init_handlers();
    }
    
    static void init_handlers() {
        if (initialized_) return;
        
        for (int i = 0; i < 256; ++i) handlers_[i] = &handle_invalid;
        
        handlers_[static_cast<unsigned char>(' ')] = &handle_skip;
        handlers_[static_cast<unsigned char>('\t')] = &handle_skip;
        handlers_[static_cast<unsigned char>('\n')] = &handle_skip;
        handlers_[static_cast<unsigned char>('\r')] = &handle_skip;
        handlers_[static_cast<unsigned char>('{')] = &handle_object_start;
        handlers_[static_cast<unsigned char>('}')] = &handle_object_end;
        handlers_[static_cast<unsigned char>('[')] = &handle_array_start;
        handlers_[static_cast<unsigned char>(']')] = &handle_array_end;
        handlers_[static_cast<unsigned char>(':')] = &handle_colon;
        handlers_[static_cast<unsigned char>(',')] = &handle_comma;
        handlers_[static_cast<unsigned char>('"')] = &handle_string;
        handlers_[static_cast<unsigned char>('t')] = &handle_true;
        handlers_[static_cast<unsigned char>('f')] = &handle_false;
        handlers_[static_cast<unsigned char>('n')] = &handle_null;
        handlers_[static_cast<unsigned char>('-')] = &handle_number;
        for (int c = '0'; c <= '9'; ++c) handlers_[c] = &handle_number;
        
        initialized_ = true;
    }
    
    JsonToken next_token() {
        while (pos_ < input_.size()) {
            unsigned char c = static_cast<unsigned char>(input_[pos_]);
            JsonToken result = handlers_[c](*this);
            if (result != JsonToken::Whitespace) {
                return result;
            }
        }
        return JsonToken::End;
    }
    
    void reset() { pos_ = 0; }

private:
    static JsonToken handle_skip(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::Whitespace;
    }
    
    static JsonToken handle_object_start(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::ObjectStart;
    }
    
    static JsonToken handle_object_end(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::ObjectEnd;
    }
    
    static JsonToken handle_array_start(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::ArrayStart;
    }
    
    static JsonToken handle_array_end(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::ArrayEnd;
    }
    
    static JsonToken handle_colon(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::Colon;
    }
    
    static JsonToken handle_comma(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::Comma;
    }
    
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
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') {
                ++t.pos_;
            } else {
                break;
            }
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
    
    static JsonToken handle_invalid(DirectThreadedTokenizer& t) {
        ++t.pos_;
        return JsonToken::Invalid;
    }
    
    std::string_view input_;
    size_t pos_;
    
    inline static bool initialized_ = false;
    inline static TokenHandler handlers_[256];
};

// ----------------------------------------------------------------------------
// Method 4: Computed Goto State Machine
// ----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
class ComputedGotoTokenizer {
public:
    explicit ComputedGotoTokenizer(std::string_view input)
        : input_(input), pos_(0) {}
    
    JsonToken next_token() {
        #define DISPATCH() goto *dispatch_table[static_cast<unsigned char>(input_[pos_])]
        
        static void* dispatch_table[256];
        static bool table_init = false;
        
        if (!table_init) [[unlikely]] {
            for (int i = 0; i < 256; ++i) dispatch_table[i] = &&L_invalid;
            dispatch_table[static_cast<unsigned char>(' ')] = &&L_skip;
            dispatch_table[static_cast<unsigned char>('\t')] = &&L_skip;
            dispatch_table[static_cast<unsigned char>('\n')] = &&L_skip;
            dispatch_table[static_cast<unsigned char>('\r')] = &&L_skip;
            dispatch_table[static_cast<unsigned char>('{')] = &&L_object_start;
            dispatch_table[static_cast<unsigned char>('}')] = &&L_object_end;
            dispatch_table[static_cast<unsigned char>('[')] = &&L_array_start;
            dispatch_table[static_cast<unsigned char>(']')] = &&L_array_end;
            dispatch_table[static_cast<unsigned char>(':')] = &&L_colon;
            dispatch_table[static_cast<unsigned char>(',')] = &&L_comma;
            dispatch_table[static_cast<unsigned char>('"')] = &&L_string;
            dispatch_table[static_cast<unsigned char>('t')] = &&L_true;
            dispatch_table[static_cast<unsigned char>('f')] = &&L_false;
            dispatch_table[static_cast<unsigned char>('n')] = &&L_null;
            dispatch_table[static_cast<unsigned char>('-')] = &&L_number;
            for (int c = '0'; c <= '9'; ++c) dispatch_table[c] = &&L_number;
            table_init = true;
        }
        
        if (pos_ >= input_.size()) return JsonToken::End;
        DISPATCH();
        
    L_skip:
        ++pos_;
        if (pos_ >= input_.size()) return JsonToken::End;
        DISPATCH();
        
    L_object_start: ++pos_; return JsonToken::ObjectStart;
    L_object_end:   ++pos_; return JsonToken::ObjectEnd;
    L_array_start:  ++pos_; return JsonToken::ArrayStart;
    L_array_end:    ++pos_; return JsonToken::ArrayEnd;
    L_colon:        ++pos_; return JsonToken::Colon;
    L_comma:        ++pos_; return JsonToken::Comma;
        
    L_string:
        ++pos_;
        while (pos_ < input_.size()) {
            char c = input_[pos_++];
            if (c == '"') return JsonToken::String;
            if (c == '\\' && pos_ < input_.size()) ++pos_;
        }
        return JsonToken::Invalid;
        
    L_number:
        ++pos_;
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' ||
                c == '+' || c == '-') {
                ++pos_;
            } else {
                break;
            }
        }
        return JsonToken::Number;
        
    L_true:
        if (pos_ + 4 <= input_.size() &&
            input_[pos_+1] == 'r' && input_[pos_+2] == 'u' && input_[pos_+3] == 'e') {
            pos_ += 4;
            return JsonToken::True;
        }
        ++pos_;
        return JsonToken::Invalid;
        
    L_false:
        if (pos_ + 5 <= input_.size() &&
            input_[pos_+1] == 'a' && input_[pos_+2] == 'l' &&
            input_[pos_+3] == 's' && input_[pos_+4] == 'e') {
            pos_ += 5;
            return JsonToken::False;
        }
        ++pos_;
        return JsonToken::Invalid;
        
    L_null:
        if (pos_ + 4 <= input_.size() &&
            input_[pos_+1] == 'u' && input_[pos_+2] == 'l' && input_[pos_+3] == 'l') {
            pos_ += 4;
            return JsonToken::Null;
        }
        ++pos_;
        return JsonToken::Invalid;
        
    L_invalid:
        ++pos_;
        return JsonToken::Invalid;
        
        #undef DISPATCH
    }
    
    void reset() { pos_ = 0; }

private:
    std::string_view input_;
    size_t pos_;
};
#else
using ComputedGotoTokenizer = SwitchTokenizer;
#endif

} // namespace json_tokenizer

// ============================================================================
// Part 3: VM-Style Bytecode Interpreter
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

// Method 1: Switch-Case VM
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

// Method 2: Direct Threaded VM
class DirectThreadedVM;
using VMHandler = void(*)(DirectThreadedVM&, const Instruction&);

class DirectThreadedVM {
public:
    DirectThreadedVM(std::span<const Instruction> prog, std::string_view input)
        : prog_(prog), input_(input) { init_handlers(); }
    
    static void init_handlers() {
        if (initialized_) return;
        handlers_[0] = [](DirectThreadedVM& vm, const Instruction&) { ++vm.pc_; };
        handlers_[1] = [](DirectThreadedVM& vm, const Instruction&) {
            while (vm.pos_ < vm.input_.size() &&
                   (vm.input_[vm.pos_] == ' ' || vm.input_[vm.pos_] == '\t' ||
                    vm.input_[vm.pos_] == '\n' || vm.input_[vm.pos_] == '\r')) ++vm.pos_;
            ++vm.pc_;
        };
        handlers_[2] = [](DirectThreadedVM& vm, const Instruction& i) {
            vm.match_ = (vm.pos_ < vm.input_.size() && vm.input_[vm.pos_] == static_cast<char>(i.operand1));
            if (vm.match_) ++vm.pos_;
            ++vm.pc_;
        };
        handlers_[3] = [](DirectThreadedVM& vm, const Instruction& i) {
            vm.match_ = (vm.pos_ < vm.input_.size() &&
                        vm.input_[vm.pos_] >= static_cast<char>(i.operand1) &&
                        vm.input_[vm.pos_] <= static_cast<char>(i.operand2));
            if (vm.match_) ++vm.pos_;
            ++vm.pc_;
        };
        handlers_[4] = [](DirectThreadedVM& vm, const Instruction& i) {
            vm.pc_ = vm.match_ ? i.operand1 : vm.pc_ + 1;
        };
        handlers_[5] = [](DirectThreadedVM& vm, const Instruction& i) {
            vm.pc_ = vm.match_ ? vm.pc_ + 1 : i.operand1;
        };
        handlers_[6] = [](DirectThreadedVM& vm, const Instruction& i) { vm.pc_ = i.operand1; };
        handlers_[7] = [](DirectThreadedVM& vm, const Instruction& i) {
            vm.result_ = static_cast<JsonToken>(i.operand1);
            vm.halted_ = true;
        };
        handlers_[8] = [](DirectThreadedVM& vm, const Instruction&) { vm.halted_ = true; };
        initialized_ = true;
    }
    
    JsonToken run() {
        pc_ = pos_ = 0;
        match_ = halted_ = false;
        result_ = JsonToken::End;
        
        while (pc_ < prog_.size() && !halted_) {
            const auto& inst = prog_[pc_];
            handlers_[static_cast<int>(inst.op)](*this, inst);
        }
        return result_;
    }

private:
    std::span<const Instruction> prog_;
    std::string_view input_;
    size_t pc_ = 0, pos_ = 0;
    bool match_ = false, halted_ = false;
    JsonToken result_ = JsonToken::End;
    
    inline static bool initialized_ = false;
    inline static VMHandler handlers_[static_cast<int>(OpCode::Count)];
};

// Method 3: Computed Goto VM
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
#else
using ComputedGotoVM = SwitchVM;
#endif

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
// Part 4: Test Cases
// ============================================================================

namespace tests {

void run_all_tests() {
    std::cout << "Running tests...\n";
    
    // Test 1: Character classification
    {
        using namespace char_classify;
        bool all_ok = true;
        
        auto test = [&](char c, CharClass expected) {
            bool ok = classify_switch(c) == expected &&
                      classify_table(c) == expected &&
                      classify_direct_threaded(c) == expected &&
                      classify_computed_goto(c) == expected &&
                      classify_hybrid(c) == expected;
            if (!ok) {
                std::cerr << "FAILED: char '" << c << "'\n";
                all_ok = false;
            }
        };
        
        test(' ', CharClass::Whitespace);
        test('0', CharClass::Digit);
        test('a', CharClass::Alpha);
        test('e', CharClass::Exponent);
        test('"', CharClass::Quote);
        test('{', CharClass::OpenBrace);
        
        std::cout << "  Character classification: " << (all_ok ? "PASSED" : "FAILED") << "\n";
    }
    
    // Test 2: JSON Tokenizer
    {
        using namespace json_tokenizer;
        
        std::string_view json = R"({"name": "test", "value": 123, "flag": true, "data": null})";
        
        std::vector<JsonToken> expected = {
            JsonToken::ObjectStart, JsonToken::String, JsonToken::Colon, JsonToken::String,
            JsonToken::Comma, JsonToken::String, JsonToken::Colon, JsonToken::Number,
            JsonToken::Comma, JsonToken::String, JsonToken::Colon, JsonToken::True,
            JsonToken::Comma, JsonToken::String, JsonToken::Colon, JsonToken::Null,
            JsonToken::ObjectEnd, JsonToken::End
        };
        
        auto test_tokenizer = [&](auto& tokenizer, const char* name) {
            std::vector<JsonToken> tokens;
            JsonToken tok;
            while ((tok = tokenizer.next_token()) != JsonToken::End) {
                tokens.push_back(tok);
                if (tokens.size() > 100) {
                    std::cout << "    " << name << ": FAILED (infinite loop)\n";
                    return false;
                }
            }
            tokens.push_back(JsonToken::End);
            
            bool ok = (tokens == expected);
            if (!ok) {
                std::cout << "    " << name << ": FAILED\n";
                std::cout << "      Got " << tokens.size() << " tokens, expected " << expected.size() << "\n";
                for (size_t i = 0; i < std::min(tokens.size(), expected.size()); ++i) {
                    if (tokens[i] != expected[i]) {
                        std::cout << "      Diff at " << i << ": got " << token_name(tokens[i]) 
                                  << ", expected " << token_name(expected[i]) << "\n";
                    }
                }
            } else {
                std::cout << "    " << name << ": PASSED\n";
            }
            return ok;
        };
        
        TableTokenizer::init_tables();
        DirectThreadedTokenizer::init_handlers();
        
        SwitchTokenizer switch_tok(json);
        TableTokenizer table_tok(json);
        DirectThreadedTokenizer direct_tok(json);
        ComputedGotoTokenizer goto_tok(json);
        
        test_tokenizer(switch_tok, "SwitchTokenizer");
        test_tokenizer(table_tok, "TableTokenizer");
        test_tokenizer(direct_tok, "DirectThreadedTokenizer");
        test_tokenizer(goto_tok, "ComputedGotoTokenizer");
    }
    
    // Test 3: VM
    {
        using namespace json_vm;
        
        auto program = make_number_program();
        std::string_view input = "  -12345";
        
        DirectThreadedVM::init_handlers();
        
        bool ok = true;
        ok &= (SwitchVM(program, input).run() == JsonToken::Number);
        ok &= (DirectThreadedVM(program, input).run() == JsonToken::Number);
        ok &= (ComputedGotoVM(program, input).run() == JsonToken::Number);
        
        std::cout << "  VM tests: " << (ok ? "PASSED" : "FAILED") << "\n";
    }
    
    std::cout << "All tests completed!\n\n";
}

} // namespace tests

// ============================================================================
// Part 5: Test Data Generation
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
        size_t len = 1 + (rng() % 8);
        for (size_t i = 0; i < len; ++i) s += '0' + (rng() % 10);
        if (rng() % 3 == 0) {
            s += '.';
            for (size_t i = 0; i < 1 + (rng() % 4); ++i) s += '0' + (rng() % 10);
        }
        return s;
    };
    
    result += "[\n";
    while (result.size() < size_hint - 100) {
        result += "  {";
        size_t pairs = 1 + (rng() % 5);
        for (size_t i = 0; i < pairs; ++i) {
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
    static const char chars[] = "{}[]\":,0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ \t\n-+.\\";
    for (size_t i = 0; i < size; ++i) result += chars[rng() % (sizeof(chars) - 1)];
    return result;
}

inline std::string small_json, medium_json, large_json, random_chars;

void init_test_data() {
    if (!small_json.empty()) return;
    small_json = generate_json(1024);
    medium_json = generate_json(64 * 1024);
    large_json = generate_json(1024 * 1024);
    random_chars = generate_random_chars(1024 * 1024);
    std::cout << "Test data: small=" << small_json.size() << "B, medium=" 
              << medium_json.size() << "B, large=" << large_json.size() << "B\n\n";
}

} // namespace test_data

// ============================================================================
// Part 6: Benchmarks
// ============================================================================

// Character Classification
static void BM_CharClassify_Switch(benchmark::State& state) {
    test_data::init_test_data();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_switch(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Switch);

static void BM_CharClassify_Table(benchmark::State& state) {
    test_data::init_test_data();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_table(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Table);

static void BM_CharClassify_DirectThreaded(benchmark::State& state) {
    test_data::init_test_data();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_direct_threaded(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_DirectThreaded);

static void BM_CharClassify_ComputedGoto(benchmark::State& state) {
    test_data::init_test_data();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_computed_goto(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_ComputedGoto);

static void BM_CharClassify_Hybrid(benchmark::State& state) {
    test_data::init_test_data();
    const auto& data = test_data::random_chars;
    for (auto _ : state) {
        uint64_t sum = 0;
        for (char c : data) sum += static_cast<uint8_t>(char_classify::classify_hybrid(c));
        benchmark::DoNotOptimize(sum);
    }
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size()));
}
BENCHMARK(BM_CharClassify_Hybrid);

// Tokenizer helpers
template<typename T>
static void tokenize_all(T& tok, uint64_t& sum) {
    JsonToken t;
    while ((t = tok.next_token()) != JsonToken::End) sum += static_cast<uint8_t>(t);
}

#define TOKENIZER_BENCH(Name, TokenizerType, DataField) \
static void BM_Tokenizer_##Name(benchmark::State& state) { \
    test_data::init_test_data(); \
    json_tokenizer::TableTokenizer::init_tables(); \
    json_tokenizer::DirectThreadedTokenizer::init_handlers(); \
    const auto& data = test_data::DataField; \
    for (auto _ : state) { \
        json_tokenizer::TokenizerType tok(data); \
        uint64_t sum = 0; \
        tokenize_all(tok, sum); \
        benchmark::DoNotOptimize(sum); \
    } \
    state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(data.size())); \
} \
BENCHMARK(BM_Tokenizer_##Name)

TOKENIZER_BENCH(Switch_Small, SwitchTokenizer, small_json);
TOKENIZER_BENCH(Table_Small, TableTokenizer, small_json);
TOKENIZER_BENCH(DirectThreaded_Small, DirectThreadedTokenizer, small_json);
TOKENIZER_BENCH(ComputedGoto_Small, ComputedGotoTokenizer, small_json);

TOKENIZER_BENCH(Switch_Medium, SwitchTokenizer, medium_json);
TOKENIZER_BENCH(Table_Medium, TableTokenizer, medium_json);
TOKENIZER_BENCH(DirectThreaded_Medium, DirectThreadedTokenizer, medium_json);
TOKENIZER_BENCH(ComputedGoto_Medium, ComputedGotoTokenizer, medium_json);

TOKENIZER_BENCH(Switch_Large, SwitchTokenizer, large_json);
TOKENIZER_BENCH(Table_Large, TableTokenizer, large_json);
TOKENIZER_BENCH(DirectThreaded_Large, DirectThreadedTokenizer, large_json);
TOKENIZER_BENCH(ComputedGoto_Large, ComputedGotoTokenizer, large_json);

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

static void BM_VM_DirectThreaded(benchmark::State& state) {
    json_vm::DirectThreadedVM::init_handlers();
    auto prog = json_vm::make_number_program();
    std::string input = "  -12345678901234567890";
    for (auto _ : state) {
        auto r = json_vm::DirectThreadedVM(prog, input).run();
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_VM_DirectThreaded);

static void BM_VM_ComputedGoto(benchmark::State& state) {
    auto prog = json_vm::make_number_program();
    std::string input = "  -12345678901234567890";
    for (auto _ : state) {
        auto r = json_vm::ComputedGotoVM(prog, input).run();
        benchmark::DoNotOptimize(r);
    }
}
BENCHMARK(BM_VM_ComputedGoto);

int main(int argc, char** argv) {
    tests::run_all_tests();
    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();
    ::benchmark::Shutdown();
    return 0;
}