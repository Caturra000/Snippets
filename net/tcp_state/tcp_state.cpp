#include <bits/stdc++.h>

struct Connection {
    int win {1000};
    int out_seq {0}; // >
    int in_seq  {0}; // <
    std::ostream &os;
    Connection(std::ostream &os): os(os) {}
    ~Connection() {os << "+.0 `sleep 1000000`\n";}
};

struct LISTEN: Connection {
    LISTEN(std::ostream &os): Connection(os) {
        os <<
            "--bind_port=8848\n"
            "  0 socket(..., SOCK_STREAM, IPPROTO_TCP) = 3\n"
            "+.0 setsockopt(3, SOL_SOCKET, SO_REUSEADDR, [1], 4) = 0\n"
            "+.0 bind(3, ..., ...) = 0\n"
            "+.0 listen(3, 128) = 0\n";
    }
};

struct SYN_SENT: Connection {
    SYN_SENT(std::ostream &os): Connection(os) {
        // 此时8848作为client，必须要bind绑定
        os <<
            "--bind_port=8848\n"
            "--connect_port=38848\n"
            "--tolerance_usecs=10000\n"
            "  0 socket(..., SOCK_STREAM, IPPROTO_TCP) = 3\n"
            "+.0 bind(3, ..., ...) = 0\n"
            "+.0 fcntl(3, F_SETFL, O_RDWR | O_NONBLOCK) = 0\n"
            "+.1 connect(3, ..., ...) = -1\n"
            "+.0 fcntl(3, F_SETFL, O_RDWR) = 0\n"
            "+.0 > S  0:0(0) <...>\n";
        out_seq++;
    }
};

struct SYN_RCVD: LISTEN {
    SYN_RCVD(std::ostream &os): LISTEN(os) {
        os << std::format(
            "+.1 < S 0:0(0) win {0}\n"
            "+.0 > S. 0:0(0) ack 1 <...>\n"
        , win);
        in_seq++;
    }
};

// 注意这个是基于被动打开而建立的
struct ESTAB: SYN_RCVD {
    ESTAB(std::ostream &os): SYN_RCVD(os) {
        os << std::format(
            "+.1 < . 1:1(0) ack 1 win {}\n"
            "+.0 accept(3, ..., ...) = 4\n"
        , win);
        out_seq++; // S. packet acked.
    }
};

struct FW1: ESTAB {
    FW1(std::ostream &os): ESTAB(os) {
        // 虽然shutdown()也能触发FW1
        // 后续收到ACK of FIN也能进入FW2
        // 但是再次发出FIN无法进入TW
        os << std::format(
            "+1  close(4) = 0\n"
        );
    }
};

struct FW2: FW1 {
    FW2(std::ostream &os): FW1(os) {
        os << std::format(
            "+.1 < . 1:1(0) ack 2 win {}\n"
        , win);
        out_seq++; // F. packet acked.
    }
};

struct TW: FW2 {
    TW(std::ostream &os): FW2(os) {
        os << std::format(
            "+.1 < F. 1:1(0) win {}\n"
        , win);
        in_seq++;
    }
};

// TODO
// struct CLOSING: FW1 {
// };

struct CW: SYN_SENT {
    CW(std::ostream &os): SYN_SENT(os) {
        os << std::format(
            "+.1 < S. 0:0(0) ack 1 win {0}\n"
            "+.0 > . 1:1(0) ack 1\n"

            "+.0 < F. 1:1(0) win {0}\n"
            "+.0 > . 1:1(0) ack 2\n"
        , win);
        in_seq += 2;
    }
};

struct LA: CW {
    LA(std::ostream &os): CW(os) {
        os <<
            "+.1 shutdown(3, SHUT_WR) = 0\n"
            "+.0 > F. 1:1(0) ack 2\n";
        out_seq++;
    }
};

//////////////////////////////////////////////////////////////////

void make_ack(auto &state) {
    state.os << std::format(
        "+.1 < . {0}:{0}(0) ack {1} win {2}\n"
    , state.in_seq, state.out_seq+1, state.win);
}

void make_older_ack(auto &state) {
    state.os << std::format(
        "+.1 < . {0}:{0}(0) ack {1} win {2}\n"
    , state.in_seq, state.out_seq, state.win);
}

void make_newer_ack(auto &state) {
    state.os << std::format(
        "+.1 < . {0}:{0}(0) ack {1} win {2}\n"
    , state.in_seq, state.out_seq+2, state.win);
}

void make_data(auto &state) {
    constexpr int len = 10;
    state.os << std::format(
        "+.1 < P. {}:{}({}) ack {} win {}\n"
    , state.in_seq, state.in_seq+len, len, state.out_seq+1, state.win);
    state.in_seq += len;
}

void make_data_random(auto &state) {
    constexpr int len = 10;
    state.os << std::format(
        "+.1 < P. {}:{}({}) ack 56789 win {}\n"
    , state.in_seq, state.in_seq+len, len, state.win);
    state.in_seq += len;
}

void make_finack(auto &state) {
    state.os << std::format(
        "+.1 < F. {0}:{0}(0) win {1}\n"
    , state.in_seq, state.win);
    state.in_seq++;
}

void make_syn(auto &state) {
    state.os << std::format(
        "+.1 < S {0}:{0}(0) win {1}\n"
    , state.in_seq, state.win);
    state.in_seq++;
}

void make_synack(auto &state) {
    state.os << std::format(
        "+.1 < S. {0}:{0}(0) ack {1} win {2}\n"
    , state.in_seq, state.out_seq+1, state.win);
    state.in_seq++;
}

void make_rst(auto &state) {
    state.os << std::format(
        "+.1 < R {0}:{0}(0) win {1}\n"
    , state.in_seq, state.win);
}

void make_null(auto &) {}

//////////////////////////////////////////////////////////////////

// rfc9293目录是未经任何修改报文，其文件名（state_names）对应于连接可达到的状态
// 其余目录会在rfc9293的基础上使用make_前缀函数构造多余的报文
template <typename T, typename Ptr = void(*)(T&)>
auto dir2func = std::unordered_map<std::string_view, Ptr> {
    {"rfc9293",     make_null},
    {"syn",         make_syn},
    {"synack",      make_synack},
    {"ack",         make_ack},
    {"older_ack",   make_older_ack},
    {"newer_ack",   make_newer_ack},
    {"data",        make_data},
    {"data_random", make_data_random},
    {"finack",      make_finack},
    {"rst",         make_rst}
};

std::string_view state_names[]{"LISTEN", "SYN_RCVD", "SYN_SENT", "ESTAB", "FW1", "FW2", "TW", "CW", "LA"};
using State_types = std::tuple< LISTEN,   SYN_RCVD,   SYN_SENT,   ESTAB,   FW1,   FW2,   TW,   CW,   LA >;

//////////////////////////////////////////////////////////////////

int main() {
    using namespace std::string_literals;
    [enable_strcat = ""s]<size_t ...Is>(std::index_sequence<Is...>) {
        ([&]<size_t I>() {
            using State = std::tuple_element_t<I, State_types>;
            for(auto &&[dir, func] : dir2func<State>) {
                std::filesystem::create_directories(dir);
                auto path = enable_strcat + dir.data() + "/" + state_names[I].data();
                std::ofstream filestream(path);
                State state(filestream);
                func(state);
            }
        }.template operator()<Is>(), ...);
    }(std::make_index_sequence<std::size(state_names)>{});
}
