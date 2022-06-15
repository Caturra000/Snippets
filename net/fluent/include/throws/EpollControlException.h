#ifndef __FLUENT_THROWS_EPOLL_CONTROL_EXCEPTION_H__
#define __FLUENT_THROWS_EPOLL_CONTROL_EXCEPTION_H__
#include "EpollException.h"
namespace fluent {

class EpollControlException: public EpollException {
public:
    static constexpr const char *TAG = "epoll control exception";
    using EpollException::EpollException;
    EpollControlException(int err): EpollException(TAG, err) {}
};

} // fluent
#endif