#ifndef __FLUENT_NETWORK_MULTIPLEXER_H__
#define __FLUENT_NETWORK_MULTIPLEXER_H__
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "../throws/Exceptions.h"
#include "Context.h"
namespace fluent {

// no any looper?
class Multiplexer {
public:
    using Token = size_t;
    // must be pointer (epoll_event.data.ptr)
    using Bundle = std::pair<Token, Context>*;
public:
    void poll(std::chrono::milliseconds timeout);

    void dispatchActiveContext(int count);

    void update(int operation, Bundle bundle);

    Token token();

    std::vector<epoll_event>& visit(Token token);

    Multiplexer();
    ~Multiplexer() { ::close(_epollFd); }
    Multiplexer(const Multiplexer&) = delete;
    Multiplexer(Multiplexer&&) = default;
    Multiplexer& operator=(const Multiplexer&) = delete;
    Multiplexer& operator=(Multiplexer&&) = default;

private:
    int _epollFd;
    std::vector<epoll_event> _events;
    std::vector<std::vector<epoll_event>> _evnetVectors;
    Token _token {0};
public:
    std::mutex _mutex;
};

inline void Multiplexer::poll(std::chrono::milliseconds timeout) {
    int count = ::epoll_wait(_epollFd, _events.data(), _events.size(), timeout.count());
    if(count < 0) {
        if(errno == EINTR) return;
        throw EpollWaitException(errno);
    }
    std::lock_guard<std::mutex> _{_mutex};
    dispatchActiveContext(count);
    if(_events.size() == count) {
        _events.reserve(_events.size() << 1);
    }
}

inline void Multiplexer::dispatchActiveContext(int count) {
    for(int i = 0; i < count; ++i) {
        size_t index = static_cast<std::pair<Token, Context>*>(_events[i].data.ptr)->first;
        _evnetVectors[index].emplace_back(_events[i]);
    }
}

inline void Multiplexer::update(int operation, Bundle bundle) {
    auto context = &bundle->second;
    int fd = context->fd();
    uint32_t events = context->events();
    epoll_event event {events, bundle};
    if(::epoll_ctl(_epollFd, operation, fd, &event)) {
        // caught by handler
        throw EpollControlException(errno);
    }
}

inline Multiplexer::Token Multiplexer::token() {
    std::lock_guard<std::mutex> _{_mutex};
    _evnetVectors.emplace_back();
    return _token++;
}

inline std::vector<epoll_event>& Multiplexer::visit(Token token) {
    return _evnetVectors[token];
}

inline Multiplexer::Multiplexer()
    : _epollFd(::epoll_create1(EPOLL_CLOEXEC)),
      _events(16) {
    if(_epollFd < 0) {
        throw EpollCreateException(errno);
    }
}

inline void Context::updateMultiplexer(Context::EpollOperationHint hint) {
    _multiplexer->update(hint, _bundle);
}

}
#endif