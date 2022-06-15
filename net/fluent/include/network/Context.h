#ifndef __FLUENT_NETWORK_CONTEXT_H__
#define __FLUENT_NETWORK_CONTEXT_H__
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../utils/Object.h"
#include "../logger/Logger.h"
#include "../policy/NetworksPolicy.h"
#include "../policy/LifecyclePolicy.h"
#include "../policy/SendPolicy.h"
#include "../policy/MultiplexerPolicy.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Buffer.h"
#include "Lifecycle.h"
namespace fluent {

class Multiplexer;

// policy base can make context interface clean (hide private/protected implementation details)
// protected inheritance: avoid the implicit casting
// using keyword marks all the public interfaces
class Context: protected NetworksPolicy,
               protected LifecyclePolicy,
               protected SendPolicy,
               protected MultiplexerPolicy {
// friends
public:
    template <typename, typename, typename> friend class Handler;

// type alias
public:
    using Completion = SendPolicy::Completion;
    using EpollOperationHint = MultiplexerPolicy::EpollOperationHint;
    using EventBitmap = MultiplexerPolicy::EventBitmap;

// class attribute
public:
    Context(Looper *looper, const InetAddress &address, Socket &&socket)
        : looper(looper),
          address(address),
          socket(std::move(socket)) {}

    Context(Looper *looper, const InetAddress &address, Socket &&socket, EnableLifecycle)
        : looper(looper),
          address(address),
          socket(std::move(socket)),
          LifecyclePolicy(EnableLifecycle{}) {}

    ~Context() = default;
    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = default;

// wrapper
public:
    int fd() const { return socket.fd(); }

// shared context
public:
    Looper *looper;
    InetAddress address;
    Socket socket;
    Buffer input;
    Buffer output;
    // captured exception
    std::exception_ptr exception;
    // user context, anything is ok
    Object any;

// networks
public:
    // A call shutdown and set DISCONNECTING ---> send FIN ---> B recv 0 ---> B handleClose ---> B shutdown and set DISCONNECTED
    // B send FIN ---> A recv 0 ---> A handleClose ---> A DISCONNECTED
    // if failed, A keeps DISCONNECTING? (B-side context not in epoll? crashed?)
    void shutdown();

    using NetworksPolicy::isConnecting;
    using NetworksPolicy::isConnected;
    using NetworksPolicy::isDisConnecting;
    using NetworksPolicy::isDisConnected;

// async, future, lifecycle
public:
    // ensure: ensureLifecycle() or EnableLifecycle
    auto makeStrongFuture() -> Future<std::tuple<Context*, StrongLifecycle>>;
    auto makeWeakFuture() -> Future<std::tuple<Context*, WeakLifecycle>>;

    // unsafe but simple future
    Future<Context*> makeFuture() { return fluent::makeFuture(looper, this); }

    using LifecyclePolicy::ensureLifecycle;
    using LifecyclePolicy::getLifecycle;

// send
public:
    Completion send(const void *buf, size_t n);
    template <size_t N>
    Completion send(const char (&buf)[N]) {  return send(buf, N-1); /*'\0'*/ }
    Completion send(const std::string &str) { return send(str.c_str(), str.size()); }

// multiplexer
public:
    using MultiplexerPolicy::updateEventState;
    using MultiplexerPolicy::events;
    using MultiplexerPolicy::readEventEnabled;
    using MultiplexerPolicy::writeEventEnabled;
    using MultiplexerPolicy::enableRead;
    using MultiplexerPolicy::enableWrite;
    using MultiplexerPolicy::disableRead;
    using MultiplexerPolicy::disableWrite;

// log helper
public:
    // trace
    uint64_t hashcode() const;
    const std::string& simpleInfo() const;

    using NetworksPolicy::networkInfo;
    using MultiplexerPolicy::eventStateInfo;
    using MultiplexerPolicy::eventsInfo;

// multiplexer(private)
private:
    void updateMultiplexer(EpollOperationHint hint);

// log helper(private)
private:
    mutable uint64_t _hashcode {};
    mutable std::string _cachedInfo;
};

/// impl

inline void Context::shutdown() {
    if(_nState == NetworkState::CONNECTED) {
        // dont care peer, DISCONNECTING can still handleClose
        _nState = NetworkState::DISCONNECTING;
        // ensure buffer is empty
        // TODO add forceClose
        if(!(_events & EVENT_WRITE)) {
            socket.shutdown();
            return;
        }
        // non-empty buffer?
        // ok, just post a request of async-shutdown
        this->makeFuture()
        .poll([](Context *context) {
            auto netInfo = context->_nState;
            bool stateValid = netInfo == NetworkState::CONNECTED;
            if(stateValid && !(context->_events & EVENT_WRITE)) {
                context->socket.shutdown();
                // ok
                return true;
            }
            if(!stateValid) {
                // disconnecting or disconnected
                // abort (duplicate request?)
                return true;
            }
            // try again in the future!
            return false;
        });
    }
}

inline auto Context::makeStrongFuture() -> Future<std::tuple<Context*, StrongLifecycle>> {
    return fluent::makeTupleFuture(looper, this, _lifecycle);
}

inline auto Context::makeWeakFuture() -> Future<std::tuple<Context*, WeakLifecycle>> {
    return fluent::makeTupleFuture(looper, this, WeakLifecycle(_lifecycle));
}

inline Context::Completion Context::send(const void *buf, size_t n) {
    if(_nState != NetworkState::DISCONNECTING && _nState != NetworkState::DISCONNECTED) {
        FLUENT_LOG_DEBUG(simpleInfo(), "tries to write", n, "bytes");
        ssize_t ret = socket.write(buf, n);
        FLUENT_LOG_DEBUG(simpleInfo(), "-->", "(stream length)", ret);
        if(ret == n) {
            // fast return
            return Completion{Completion::FAST_COMPLETE, this};
        }
        // TODO submit request to buffer with readyFlag and vector
        output.append(static_cast<const char *>(buf) + ret, n - ret);
        FLUENT_LOG_DEBUG(simpleInfo(), "-->", "(buffer length)", n - ret);
        _sendProvider.emplace(n - ret);
        size_t sendToken = _sendCurrentIndex++;
        if(!(_events & EVENT_WRITE)) {
            _events |= EVENT_WRITE;
            // TODO remove this ugly code, use handler when any callback
            auto hint = updateEventState();
            updateMultiplexer(hint);
            // will disable write when buf.unread == 0 (handleWrite)
        }
        return Completion{sendToken, this};
    }
    return Completion{Completion::INVALID, this};
}

inline uint64_t Context::hashcode() const {
    if(_hashcode) return _hashcode;
    return _hashcode = (uint64_t(socket.fd()) << 32)
        ^ uint64_t(address.rawIp())
        ^ (uint64_t(address.rawPort()) << 24);
}

inline const std::string& Context::simpleInfo() const {
    if(_cachedInfo.length()) return _cachedInfo;
    return _cachedInfo = "[" + std::to_string(socket.fd()) + ", ("
        + address.toString() + ")]";
}

} // fluent
#endif