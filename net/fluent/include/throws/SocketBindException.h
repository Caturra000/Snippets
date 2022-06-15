#ifndef __FLUENT_THROWS_SOCKET_BIND_EXCEPTION_H__
#define __FLUENT_THROWS_SOCKET_BIND_EXCEPTION_H__
#include "SocketException.h"
namespace fluent {

class SocketBindException: public SocketException {
public:
    static constexpr const char *TAG = "socket bind exception";
    using SocketException::SocketException;
    SocketBindException(int err): SocketException(TAG, err) {}
};

} // fluent
#endif