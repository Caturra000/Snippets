#ifndef __FLUENT_NETWORK_SOCKET_H__
#define __FLUENT_NETWORK_SOCKET_H__
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <bits/stdc++.h>
#include "InetAddress.h"
#include "../throws/Exceptions.h"
namespace fluent {

class Socket {
public:
    int fd() const { return _socketFd; }

    void bind(const InetAddress &address);
    void listen(int backlog = SOMAXCONN);
    Socket accept(InetAddress &clientAddress);
    Socket accept();
    int connect(const InetAddress &address);
    void detach() { _socketFd = INVALID_FD; }
    void shutdown() { ::shutdown(_socketFd, SHUT_WR); }

    void setNoDelay(bool on = true);
    void setReuseAddr(bool on = true);
    void setReusePort(bool on = true);
    void setKeepAlive(bool on = true);
    void setBlock();
    void setNonBlock();

    ssize_t write(const void *buf, size_t n);

    void swap(Socket &that) { std::swap(this->_socketFd, that._socketFd); }

    static constexpr int INVALID_FD = -1;

    Socket();
    ~Socket();
    explicit Socket(int socketFd): _socketFd(socketFd) { /*assert(_socketFd != INVALID_FD);*/ }
    Socket(const Socket&) = delete;
    Socket(Socket &&rhs): _socketFd(rhs._socketFd) { rhs._socketFd = INVALID_FD; }
    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket &&rhs);

    // int fd = std::move(socket);
    operator int() &&;

private:
    int _socketFd;
};

inline void Socket::bind(const InetAddress &address) {
    if(::bind(_socketFd, (const sockaddr*)(&address), sizeof(InetAddress))) {
        throw SocketBindException(errno);
    }
}
inline void Socket::listen(int backlog) {
    if(::listen(_socketFd, backlog)) {
        throw SocketListenException(errno);
    }
}


inline Socket Socket::accept(InetAddress &address) {
    socklen_t len = sizeof(address);
    int connectFd = ::accept4(_socketFd, (sockaddr*)(&address), &len,
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connectFd < 0) {
        throw SocketAcceptException(errno);
    }
    return Socket(connectFd);
}

inline Socket Socket::accept() {
    int connectFd = ::accept4(_socketFd, nullptr, nullptr,
                        SOCK_NONBLOCK | SOCK_CLOEXEC);
    if(connectFd < 0) {
        throw SocketAcceptException(errno);
    }
    return Socket(connectFd);
}

inline int Socket::connect(const InetAddress &address) {
    return ::connect(_socketFd, (const sockaddr *)(&address), sizeof(address));
}

inline void Socket::setNoDelay(bool on) {
    int optval = on;
    if(::setsockopt(_socketFd, IPPROTO_TCP, TCP_NODELAY, &optval,
            static_cast<socklen_t>(sizeof optval))) {
        throw SocketException(errno);
    }
}

inline void Socket::setReuseAddr(bool on) {
    int optval = on;
    if(::setsockopt(_socketFd, SOL_SOCKET, SO_REUSEADDR, &optval,
            static_cast<socklen_t>(sizeof optval))) {
        throw SocketException(errno);
    }
}

inline void Socket::setReusePort(bool on) {
    int optval = on;
    if(::setsockopt(_socketFd, SOL_SOCKET, SO_REUSEPORT, &optval,
            static_cast<socklen_t>(sizeof optval))) {
        throw SocketException(errno);
    }
}

inline void Socket::setKeepAlive(bool on) {
    int optval = on;
    if(::setsockopt(_socketFd, SOL_SOCKET, SO_KEEPALIVE, &optval,
            static_cast<socklen_t>(sizeof optval))) {
        throw SocketException(errno);
    }
}


inline void Socket::setBlock() {
    int flags = ::fcntl(_socketFd, F_GETFL, 0);
    ::fcntl(_socketFd, F_SETFL, flags & (~SOCK_NONBLOCK));
}

inline void Socket::setNonBlock() {
    int flags = ::fcntl(_socketFd, F_GETFL, 0);
    ::fcntl(_socketFd, F_SETFL, flags | SOCK_NONBLOCK);
}

inline ssize_t Socket::write(const void *buf, size_t n) {
    ssize_t ret = ::write(_socketFd, buf, n);
    if(ret < 0) {
        switch(errno) {
            case EAGAIN:
            case EINTR:
                ret = 0;
            break;
            default:
                // Log...
                throw SocketException(errno);
                // or no throw?
                // future is hard to catch exception (try-catch everytime)
                // TODO add futureSend interface
            break;
        }
    }
    return ret;
}

inline Socket::Socket()
    : _socketFd(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) {
    if(_socketFd < 0) {
        throw SocketCreateException(errno);
    }
}

inline Socket::~Socket() {
    if(_socketFd != INVALID_FD) {
        ::close(_socketFd);
    }
}

inline Socket& Socket::operator=(Socket &&rhs) {
    Socket(static_cast<Socket&&>(rhs)).swap(*this);
    return *this;
}

inline Socket::operator int() && {
    int fd = _socketFd;
    _socketFd = INVALID_FD;
    return fd;
}

} // fluent

#endif