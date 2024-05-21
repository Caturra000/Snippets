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
        out_seq++;
    }
};

// 注意这个是基于被动打开而建立的
struct ESTAB: SYN_RCVD {
    ESTAB(std::ostream &os): SYN_RCVD(os) {
        os << std::format(
            "+.1 < . 1:1(0) ack 1 win {}\n"
            "+.0 accept(3, ..., ...) = 4\n"
        , win);
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
        out_seq++; // F. packet
    }
};

struct FW2: FW1 {
    FW2(std::ostream &os): FW1(os) {
        os << std::format(
            "+.1 < . 1:1(0) ack 2 win {}\n"
        , win);
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

void make_ack(auto &base) {
    base.os << std::format(
        "+.1 < . {0}:{0}(0) ack {1} win {2}\n"
    , base.in_seq, base.out_seq+1, base.win);
}

void make_data(auto &base) {
    constexpr int len = 10;
    base.os << std::format(
        "+.1 < P. {}:{}({}) ack {} win {}\n"
    , base.in_seq, base.in_seq+len, len, base.out_seq+1, base.win);
    base.in_seq += len;
}

void make_data_random(auto &base) {
    constexpr int len = 10;
    base.os << std::format(
        "+.1 < P. {}:{}({}) ack 56789 win {}\n"
    , base.in_seq, base.in_seq+len, len, base.win);
    base.in_seq += len;
}

void make_finack(auto &base) {
    base.os << std::format(
        "+.1 < F. {0}:{0}(0) win {1}\n"
    , base.in_seq, base.win);
    base.in_seq++;
}

void make_syn(auto &base) {
    base.os << std::format(
        "+.1 < S {0}:{0}(0) win {1}\n"
    , base.in_seq, base.win);
    base.in_seq++;
}

void make_synack(auto &base) {
    base.os << std::format(
        "+.1 < S. {0}:{0}(0) ack {1} win {2}\n"
    , base.in_seq, base.out_seq+1, base.win);
    base.in_seq++;
}

void make_null(auto &) {}

//////////////////////////////////////////////////////////////////

template <typename T>
using Make_func_ptr_type = void(*)(T&);

// rfc9293目录是未经任何修改报文，其文件名（state_names）对应于连接可达到的状态
// 其余目录会在rfc9293的基础上使用make_前缀函数构造多余的报文
std::string dirs[] {"rfc9293", "synack", "ack", "data", "data_random", "finack"};

template <typename T>
Make_func_ptr_type<T> make_funcs[] {
    make_null, make_synack, make_ack, make_data, make_data_random, make_finack
};

std::string state_names[]     {"LISTEN", "SYN_RCVD", "SYN_SENT", "ESTAB", "FW1", "FW2", "TW", "CW", "LA"};
using State_types = std::tuple< LISTEN,   SYN_RCVD,   SYN_SENT,   ESTAB,   FW1,   FW2,   TW,   CW,   LA>;

//////////////////////////////////////////////////////////////////

int main() {
    [&]<size_t ...Is>(std::index_sequence<Is...>) {
        ([&]<size_t I>() {
            using T = std::tuple_element_t<I, State_types>;
            auto &funcs = make_funcs<T>;
            namespace vs = std::views;
            for(auto idx : vs::iota(0) | vs::take(std::size(funcs))) {
                std::filesystem::create_directories(dirs[idx]);
                std::ofstream out(dirs[idx] + "/" + state_names[I]);
                T conn(out);
                funcs[idx](conn);
            }
        }.template operator()<Is>(), ...);
    }(std::make_index_sequence<std::size(state_names)>{});
}
