#ifndef __FLUENT_THROWS_SOCKET_CREATE_EXCEPTION_H__
#define __FLUENT_THROWS_SOCKET_CREATE_EXCEPTION_H__
#include "SocketException.h"
namespace fluent {

class SocketCreateException: public SocketException {
public:
    static constexpr const char *TAG = "socket create exception";
    using SocketException::SocketException;
    SocketCreateException(int err)
        : SocketException(TAG, err) {}
};

} // fluent
#endif