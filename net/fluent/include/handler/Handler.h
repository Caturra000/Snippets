#ifndef __FLUENT_HANDLER_HANDLER_H__
#define __FLUENT_HANDLER_HANDLER_H__
#include <bits/stdc++.h>
#include "../network/Multiplexer.h"
#include "../network/Context.h"
#include "../logger/Logger.h"
#include "HandlerBase.h"
namespace fluent {

template <typename ConnectCallback,
          typename MessageCallback,
          typename CloseCallback>
class Handler
    : public HandlerBase<Handler<ConnectCallback, MessageCallback, CloseCallback>,
                         Multiplexer::Token,
                         Multiplexer::Bundle> {
public:
    using ConnectCallbackType = ConnectCallback;
    using MessageCallbackType = MessageCallback;
    using CloseCallbackType   = CloseCallback;

    using Token = Multiplexer::Token;
    using Bundle = Multiplexer::Bundle;

    using ThisType = Handler<ConnectCallback, MessageCallback, CloseCallback>;
    using Base = HandlerBase<ThisType, Token, Bundle>;

public:
    // side effect: event vector .clear()
    void handleEvents(Token token);

    void handleNewContext(Bundle bundle);

// require
public:
    void handleRead(Bundle bundle);

    // eusure: write event
    void handleWrite(Bundle bundle);

    // connected/disconnecting -> disconnecting -> disconnected
    // if call shutdown manually -> disconnecting
    void handleClose(Bundle bundle);


    // TODO handleError(Context *context, const char *exceptionMessage) // dont use std::string, .c_str() is unsafe
    // TODO FluentException +std::string
    void handleError(Bundle bundle);
    void handleError(Bundle bundle, const char *errorMessage);

// register
public:
    void onConnect(ConnectCallback callback) { _connectCallback = std::move(callback); _connectFlag = true; }
    void onMessage(MessageCallback callback) { _messageCallback = std::move(callback); _messageFlag = true;}
    void onClose(CloseCallback callback) { _closeCallback = std::move(callback); _closeFlag = true; }

public:
    Handler() = delete;
    Handler(Multiplexer *multiplexer)
        : _multiplexer(multiplexer),
          _connectFlag(false),
          _messageFlag(false),
          _closeFlag(false) {}
    Handler(Multiplexer *multiplexer,
            ConnectCallback connectCallback,
            MessageCallback messageCallback,
            CloseCallback   closeCallback)
        : _multiplexer(multiplexer),
          _connectCallback(std::move(connectCallback)),
          _messageCallback(std::move(messageCallback)),
          _closeCallback(std::move(closeCallback)),
          _connectFlag(true),
          _messageFlag(true),
          _closeFlag(true) {}

    ~Handler() = default;

    Handler(const Handler&) = default;
    Handler(Handler&&) = default;
    Handler& operator=(const Handler&) = default;
    Handler& operator=(Handler&&) = default;

private:
    void handleWriteComplete(Context *context, ssize_t n);

private:
    Multiplexer     *_multiplexer;
    ConnectCallback _connectCallback;
    MessageCallback _messageCallback;
    CloseCallback   _closeCallback;
    std::vector<epoll_event> _eventBuffer;
    // template functor cannot cast bool(f) to check flag (std::function is fine)
    bool _connectFlag;
    bool _messageFlag;
    bool _closeFlag;
};

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleEvents(Token token) {
    {
        std::lock_guard<std::mutex> _{_multiplexer->_mutex};
        auto &eventVector = _multiplexer->visit(token);
        std::swap(eventVector, _eventBuffer);
    }
    for(auto &event : _eventBuffer) {
        auto bundle = static_cast<Bundle>(event.data.ptr);
        auto revent = event.events;
        try {
            Base::handleEvent(bundle, revent);
        } catch(const std::exception &e) {
            auto context = &bundle->second;
            FLUENT_LOG_WARN(context->simpleInfo(), "catches an exception:", e.what());
            FLUENT_LOG_DEBUG("context handling:", context->simpleInfo(),
                context->networkInfo(), context->eventStateInfo(), context->eventsInfo());
            context->exception = std::current_exception();
            // TODO _exceptionCallback(context);
        }
    }
    _eventBuffer.clear();
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleNewContext(Bundle bundle) try {
    auto context = &bundle->second;
    FLUENT_LOG_DEBUG(context->simpleInfo(), "connecting");
    // ensure
    context->_multiplexer = _multiplexer;
    context->_bundle = bundle;
    // assert CONNECTING
    context->_nState = Context::NetworkState::CONNECTED;
    context->enableRead();
    Context::EpollOperationHint operation;
    if((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE) {
        _multiplexer->update(operation, bundle);
    }
    FLUENT_LOG_INFO(context->simpleInfo(), "connected. [HASHCODE]", context->hashcode());
    if(_connectFlag) _connectCallback(context);
} catch(const std::exception &e) {
    auto context = &bundle->second;
    FLUENT_LOG_WARN(context->simpleInfo(), "catches an exception:", e.what());
    context->exception = std::current_exception();
    // TODO _exceptionCallback(context);
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleRead(Bundle bundle) {
    auto context = &bundle->second;
    ssize_t n = context->input.readFrom(context->socket.fd());
    FLUENT_LOG_DEBUG(context->simpleInfo(), "<--", "(stream length)", n);
    if(n > 0) {
        if(_messageFlag) _messageCallback(context);
    } else if(n == 0) {
        // TODO add: const char *callbackReason / size_t reasonCode in Context
        // TODO handleClose(context, "reason...")
        FLUENT_LOG_INFO(context->simpleInfo(), "<--", "FIN");
        handleClose(bundle);
    } else if(errno != EAGAIN && errno != EINTR) {
        handleError(bundle);
    }
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleWrite(Bundle bundle) {
    auto context = &bundle->second;
    if(context->writeEventEnabled()) {
        ssize_t n = context->output.writeTo(context->socket.fd());
        FLUENT_LOG_DEBUG(context->simpleInfo(), "-->", "(stream length)", n);
        // assert n >= 0
        handleWriteComplete(context, n);
        if(n > 0 && context->output.unread() == 0) {
            Context::EpollOperationHint operation;
            if(context->disableWrite()
                    && ((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE)) {
                _multiplexer->update(operation, bundle);
            }
        }
    }
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleClose(Bundle bundle) {
    auto context = &bundle->second;
    if(context->isConnected() || context->isDisConnecting()) {
        // try to shutdown if force close
        context->shutdown();
        // shouble be DISCONNECTING
        context->disableRead();
        context->disableWrite();
        Context::EpollOperationHint operation;
        if((operation = context->updateEventState()) != Context::EPOLL_CTL_NONE) {
            _multiplexer->update(operation, bundle);
        }
        context->_nState = Context::NetworkState::DISCONNECTED;
        if(_closeFlag) _closeCallback(context);
        FLUENT_LOG_INFO(context->simpleInfo(), "close. [HASHCODE]", context->hashcode());
    }
}


template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleError(Bundle bundle) {
    // auto context = &bundle->second;
    // try {
    //     throw FluentException("error callback");
    // } catch(...) {
    //     context->exception = std::current_exception();
    // }
    FLUENT_LOG_WARN((&bundle->second)->simpleInfo(), "error callback!");
    throw FluentException("error callback");
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleError(Bundle bundle, const char *errorMessage) {
    FLUENT_LOG_WARN((&bundle->second)->simpleInfo(), errorMessage);
    throw FluentException(errorMessage);
}

template <typename ConnectCallback, typename MessageCallback, typename CloseCallback>
inline void Handler<ConnectCallback, MessageCallback, CloseCallback>::handleWriteComplete(Context *context, ssize_t n) {
    auto &provider = context->_sendProvider;
    while(n > 0 && !provider.empty()) {
        if(provider.front() > n) {
            provider.front() -= n;
            break;
        }
        // else
        n -= provider.front();
        provider.pop();
        context->_sendCompletedIndex++;
    }
}

} // fluent
#endif