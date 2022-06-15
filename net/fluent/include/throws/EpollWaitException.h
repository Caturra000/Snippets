#ifndef __FLUENT_THROWS_EPOLL_WAIT_EXCEPTION_H__
#define __FLUENT_THROWS_EPOLL_WAIT_EXCEPTION_H__
#include "EpollException.h"
namespace fluent {

class EpollWaitException: public EpollException {
public:
    static constexpr const char *TAG = "epoll create exception";
    using EpollException::EpollException;
    EpollWaitException(int err): EpollException(TAG, err) {}
};

} // fluent
#endif