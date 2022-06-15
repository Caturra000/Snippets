#ifndef __FLUENT_APP_SERVER_H__
#define __FLUENT_APP_SERVER_H__
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "../network/Context.h"
#include "../network/Acceptor.h"
#include "../network/Multiplexer.h"
#include "../network/Pool.h"
#include "../handler/Handler.h"
namespace fluent {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer;

using Server = BaseServer<std::function<void(Context*)>,
                          std::function<void(Context*)>,
                          std::function<void(Context*)>>;

// factory function
// help template argument deduction
template <typename ConnectCallbackForward,
          typename MessageCallbackForward,
          typename CloseCallbackForward,
          /// return type
          typename ConnectCallback = typename std::remove_reference<ConnectCallbackForward>::type,
          typename MessageCallback = typename std::remove_reference<MessageCallbackForward>::type,
          typename CloseCallbackd  = typename std::remove_reference<CloseCallbackForward>::type>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallbackd>
makeServer(const InetAddress      &address,
           ConnectCallbackForward &&connectCallback,
           MessageCallbackForward &&messageCallback,
           CloseCallbackForward   &&closeCallback) {
    return BaseServer<ConnectCallback, MessageCallback, CloseCallbackd>(
            address,
            std::forward<ConnectCallbackForward>(connectCallback),
            std::forward<MessageCallbackForward>(messageCallback),
            std::forward<CloseCallbackForward>(closeCallback)
        );
}

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class BaseServer {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;
    using HandlerType = Handler<ConnectCallback, MessageCallback, CloseCallback>;

public:
    void run() { for(_stop = false; !_stop; ) batch(); }
    void batch();

    // TODO stop acceptor event
    void ready() { _acceptor.start(); }
    void stop() { _stop = true; }

    Looper* looper() { return &_looper; }

    void onConnect(ConnectCallback callback) { _handler.onConnect(std::move(callback)); }
    void onMessage(MessageCallback callback) { _handler.onMessage(std::move(callback)); }
    void onClose(CloseCallback callback) { _handler.onClose(std::move(callback)); }

    BaseServer(InetAddress address);
    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer);
    BaseServer(InetAddress address,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    BaseServer(InetAddress address,
               std::shared_ptr<Multiplexer> multiplexer,
               ConnectCallback connectCallback,
               MessageCallback messageCallback,
               CloseCallback   closeCallback);
    ~BaseServer() = default;
    BaseServer(const BaseServer&) = delete;
    BaseServer(BaseServer&&) = default;
    BaseServer& operator=(const BaseServer&) = delete;
    BaseServer& operator=(BaseServer&&) = default;

private:
    void accept();

private:
    Looper _looper;

    std::shared_ptr<Multiplexer> _multiplexer;
    bool _isOuterMultiplexer;
    const Multiplexer::Token _token;

    Acceptor _acceptor;
    HandlerType _handler;
    Pool _connections;

    bool _stop {false};
};

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseServer<ConnectCallback, MessageCallback, CloseCallback>::batch() {
    if(!_isOuterMultiplexer) {
        _multiplexer->poll(std::chrono::milliseconds::zero());
    }
    accept();
    _handler.handleEvents(_token);
    _looper.loop();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address)
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address,
           std::shared_ptr<Multiplexer> multiplexer)
    : _multiplexer(std::move(multiplexer)),
      _isOuterMultiplexer(true),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get()) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address,
           ConnectCallback connectCallback,
           MessageCallback messageCallback,
           CloseCallback   closeCallback)
    : _multiplexer(std::make_shared<Multiplexer>()),
      _isOuterMultiplexer(false),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline BaseServer<ConnectCallback, MessageCallback, CloseCallback>::
BaseServer(InetAddress address,
          std::shared_ptr<Multiplexer> multiplexer,
          ConnectCallback connectCallback,
          MessageCallback messageCallback,
          CloseCallback   closeCallback)
    : _multiplexer(std::move(multiplexer)),
      _isOuterMultiplexer(true),
      _token(_multiplexer->token()),
      _acceptor(&_looper, address),
      _handler(_multiplexer.get(), std::move(connectCallback), std::move(messageCallback), std::move(closeCallback)) {}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void BaseServer<ConnectCallback, MessageCallback, CloseCallback>::accept() {
    if(_acceptor.accept()) {
        std::pair<InetAddress, Socket> contextArguments = _acceptor.aceeptResult();
        auto &address = contextArguments.first;
        auto &socket = contextArguments.second;
        Multiplexer::Bundle bundle = _connections.emplace(_token, &_looper, address, std::move(socket));
        _handler.handleNewContext(bundle);
    }
}

} // fluent
#endif