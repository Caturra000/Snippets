#ifndef __FLUENT_THROWS_SOCKET_EXCEPTION_H__
#define __FLUENT_THROWS_SOCKET_EXCEPTION_H__
#include "NetworkException.h"
namespace fluent {

class SocketException: public NetworkException {
public:
    static constexpr const char *TAG = "socket exception";
    using NetworkException::NetworkException;
    SocketException(int err): NetworkException(TAG, err) {}
};

} // fluent
#endif