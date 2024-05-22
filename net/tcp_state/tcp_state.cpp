#include <bits/stdc++.h>

struct Connection {
    int win {1000};
    int out_seq {0}; // >
    int in_seq  {0}; // <
    std::ostream &os;
    Connection(std::ostream &os): os(os) {}
    ~Connection() {os << "+.0 `sleep 1000000`\n";}
};

// 为了避免在构造函数里拉面条
void init(auto &state);

template <std::derived_from<Connection> Source, auto UNIQUE_using = [](){}>
struct From: Source {
    From(std::ostream &os): Source(os) { init(*this); }
};

using LISTEN   = From<Connection>;
using SYN_RCVD = From<LISTEN>;
using ESTAB    = From<SYN_RCVD>;
using FW1      = From<ESTAB>;
using FW2      = From<FW1>;
using TW       = From<FW2>;

// SYN_SENT和LISTEN并非同一类型
using SYN_SENT = From<Connection>;
using CW       = From<SYN_SENT>;
using LA       = From<CW>;

void init(LISTEN &state) {
    state.os <<
        "--bind_port=8848\n"
        "  0 socket(..., SOCK_STREAM, IPPROTO_TCP) = 3\n"
        "+.0 setsockopt(3, SOL_SOCKET, SO_REUSEADDR, [1], 4) = 0\n"
        "+.0 bind(3, ..., ...) = 0\n"
        "+.0 listen(3, 128) = 0\n";
}

void init(SYN_RCVD &state) {
    state.os << std::format(
        "+.1 < S 0:0(0) win {0}\n"
        "+.0 > S. 0:0(0) ack 1 <...>\n"
    , state.win);
    state.in_seq++;
}

// 注意这是基于被动打开的连接
void init(ESTAB &state) {
    state.os << std::format(
        "+.1 < . 1:1(0) ack 1 win {}\n"
        "+.0 accept(3, ..., ...) = 4\n"
    , state.win);
    state.out_seq++; // S. packet acked.
}

void init(FW1 &state) {
    // 虽然shutdown()也能触发FW1
    // 后续收到ACK of FIN也能进入FW2
    // 但是再次发出FIN无法进入TW，而close()可以
    state.os << std::format(
        "+1  close(4) = 0\n"
    );
}

void init(FW2 &state) {
    state.os << std::format(
        "+.1 < . 1:1(0) ack 2 win {}\n"
    , state.win);
    state.out_seq++; // F. packet acked.
}

void init(TW &state) {
    state.os << std::format(
        "+.1 < F. 1:1(0) win {}\n"
    , state.win);
    state.in_seq++;
}

void init(SYN_SENT &state) {
    // 此时8848作为client，必须要bind绑定
    state.os <<
        "--bind_port=8848\n"
        "--connect_port=38848\n"
        "--tolerance_usecs=10000\n"
        "  0 socket(..., SOCK_STREAM, IPPROTO_TCP) = 3\n"
        "+.0 bind(3, ..., ...) = 0\n"
        "+.0 fcntl(3, F_SETFL, O_RDWR | O_NONBLOCK) = 0\n"
        "+.1 connect(3, ..., ...) = -1\n"
        "+.0 fcntl(3, F_SETFL, O_RDWR) = 0\n"
        "+.0 > S  0:0(0) <...>\n";
    state.out_seq++;
}

void init(CW &state) {
    state.os << std::format(
        "+.1 < S. 0:0(0) ack 1 win {0}\n"
        "+.0 > . 1:1(0) ack 1\n"

        "+.0 < F. 1:1(0) win {0}\n"
        "+.0 > . 1:1(0) ack 2\n"
    , state.win);
    state.in_seq += 2;
}

void init(LA &state) {
    state.os <<
        "+.1 shutdown(3, SHUT_WR) = 0\n"
        "+.0 > F. 1:1(0) ack 2\n";
    state.out_seq++;
}

//////////////////////////////////////////////////////////////////

constexpr struct make_option {
    char flag {' '};
    int  len {0};
    bool is_logical_seq {true};
    bool ack {true};
    bool random_ack {false};
    int  addend_ack {0};
} pure_ack_packet {.is_logical_seq=false};

template <make_option option = pure_ack_packet>
void make_single_packet(auto &state) {
    char ack_flag = option.ack ? '.': ' ';
    std::string ack_str;
    if(option.ack) {
        int ack_num = (option.random_ack ? 19260817 : state.out_seq)
                    +  option.addend_ack;
        ack_str = std::format("ack {}", ack_num);
    }

    state.os << std::format(
        "+.1 < {}{} {}:{}({}) {} win {}\n"
    , option.flag, ack_flag
    , state.in_seq, state.in_seq+option.len, option.len
    , ack_str, state.win);

    if(option.len || option.is_logical_seq) {
        state.in_seq += std::max(option.len, 1);
    }
}

template <auto ...options>
void make_packet(auto &state) {
    (make_single_packet<options>(state), ...);
}

//////////////////////////////////////////////////////////////////

// rfc9293目录是未经任何修改报文，其文件名（states）对应于连接可达到的状态
// 其余目录会在rfc9293的基础上使用make_前缀函数构造多余的报文
template <typename T, typename Ptr = void(*)(T&)>
auto dir2func = std::unordered_map<std::string_view, Ptr> {
    {"rfc9293",     make_packet},
    {"ack",         make_packet<pure_ack_packet>},
    {"synack",      make_packet<make_option{.flag='S'}>},
    {"syn",         make_packet<make_option{.flag='S', .ack=false}>},
    {"finack",      make_packet<make_option{.flag='F'}>},
    {"fin",         make_packet<make_option{.flag='F', .ack=false}>},
    {"rstack",      make_packet<make_option{.flag='R'}>},
    {"rst",         make_packet<make_option{.flag='R', .ack=false}>},
    {"data",        make_packet<make_option{.flag='P', .len=10}>},
    {"data_random", make_packet<make_option{.flag='P', .len=10, .random_ack=true}>},
    {"older_ack",   make_packet<make_option{.is_logical_seq=false, .addend_ack=-1}>},
    {"newer_ack",   make_packet<make_option{.is_logical_seq=false, .addend_ack=+1}>},
    {"note1",       make_packet<make_option{.flag='S', .ack=false}, make_option{.flag='R', .ack=false}>}
};

std::string_view states[]{"LISTEN", "SYN_RCVD", "SYN_SENT", "ESTAB", "FW1", "FW2", "TW", "CW", "LA"};
using State_types =
               std::tuple< LISTEN,   SYN_RCVD,   SYN_SENT,   ESTAB,   FW1,   FW2,   TW,   CW,   LA >;

//////////////////////////////////////////////////////////////////

int main() {
    using namespace std::string_literals;
    [enable_strcat = ""s]<size_t ...Is>(std::index_sequence<Is...>) {
        ([&]<size_t I>() {
            using State = std::tuple_element_t<I, State_types>;
            for(auto &&[dir, func] : dir2func<State>) {
                std::filesystem::create_directories(dir);
                auto path = enable_strcat + dir.data() + "/" + states[I].data();
                std::ofstream filestream(path);
                State state(filestream);
                func(state);
            }
        }.template operator()<Is>(), ...);
    }(std::make_index_sequence<std::size(states)>{});
}
