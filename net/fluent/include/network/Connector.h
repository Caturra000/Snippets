#ifndef __FLUENT_NETWORK_CONNECTOR_H__
#define __FLUENT_NETWORK_CONNECTOR_H__
#include <unistd.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <bits/stdc++.h>
#include "../future/Futures.h"
#include "InetAddress.h"
#include "Socket.h"
// #include "Multiplexer.h"
namespace fluent {

class Connector {
public:
    Future<Connector*> makeFuture() { return fluent::makeFuture(_looper, this); }

    Future<std::pair<InetAddress, Socket>> connect(const InetAddress /*server*/&address);

    Connector(Looper *looper) : _looper(looper) {}
    ~Connector() = default;
    Connector(const Connector&) = delete;
    Connector(Connector&&) = default;
    Connector& operator=(const Connector&) = delete;
    Connector& operator=(Connector&&) = default;

private:
    static int testFailed(const Socket &socket);

private:
    Looper *_looper;
};

inline Future<std::pair<InetAddress, Socket>> Connector::connect(const InetAddress /*server*/&address) {
    Socket socket;
    // try to connect immediately, then test in async-task
    int ret = socket.connect(address);
    int err = ret ? errno : 0;
    bool reconnect = false;
    bool nextInstance = false;
    auto arguments = std::make_tuple(address, std::move(socket), err, reconnect, nextInstance);
    // safe lifecycle
    auto constructor = std::make_shared<decltype(arguments)>(std::move(arguments));
    return this->makeFuture()
        .poll([constructor](Connector*) {
            auto &arguments = *constructor;
            Socket &socket = std::get<1>(arguments);
            int &err = std::get<2>(arguments);
            bool &reconnect = std::get<3>(arguments);
            bool &nextInstance = std::get<4>(arguments);
            // unlikely
            if(reconnect) {
                InetAddress &address = std::get<0>(arguments);
                if(nextInstance) {
                    Socket().swap(socket);
                    nextInstance = false;
                }
                int ret = socket.connect(address);
                err = ret ? errno : 0;
                reconnect = false;
                return !err;
            }
            switch(err) {
                case 0:
                    return true;
                case EINTR:
                case ENETUNREACH:
                case ETIMEDOUT:
                    nextInstance = true;
                    reconnect = true;
                    return false;
                case EINPROGRESS:
                case EISCONN:
                case EALREADY:
                case ECONNREFUSED:
                    // example:
                    // EINPROGRESS -> EALREADY -> ECONNREFUSED -> connected
                    if(err = Connector::testFailed(socket)) {
                        reconnect = true;
                        return false;
                    }
                    // err == 0
                    return true;
                default:
                    // err != 0
                    return true;
            }
        })
        .cancelIf([constructor](Connector*) {
            auto &args = *constructor;
            int err = std::get<2>(args);
            // TODO LOG_WARN reason: strerror(err)
            // TODO cancel and then execute next user-defined callback
            return !!err;
        })
        .then([constructor](Connector*) {
            auto &arguments = *constructor;
            InetAddress &address = std::get<0>(arguments);
            Socket &socket = std::get<1>(arguments);
            return std::make_pair(address, std::move(socket));
        });
}

inline int Connector::testFailed(const Socket &socket) {
    sockaddr jojo;
    socklen_t len = sizeof(jojo);
    if(::getpeername(socket.fd(), &jojo, &len)) {
        return errno;
    }
    return 0;
}

} // fluent
#endif