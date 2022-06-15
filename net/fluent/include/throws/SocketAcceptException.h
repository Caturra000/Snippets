#ifndef __FLUENT_THROWS_SOCKET_ACCEPT_EXCEPTION_H__
#define __FLUENT_THROWS_SOCKET_ACCEPT_EXCEPTION_H__
#include "SocketException.h"
namespace fluent {

class SocketAcceptException: public SocketException {
public:
    static constexpr const char *TAG = "socket accept exception";
    using SocketException::SocketException;
    SocketAcceptException(int err): SocketException(TAG, err) {}
};

} // fluent
#endif