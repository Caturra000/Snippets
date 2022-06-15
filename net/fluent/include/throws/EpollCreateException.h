#ifndef __FLUENT_THROWS_EPOLL_CREATE_EXCEPTION_H__
#define __FLUENT_THROWS_EPOLL_CREATE_EXCEPTION_H__
#include "EpollException.h"
namespace fluent {

class EpollCreateException: public EpollException {
public:
    static constexpr const char *TAG = "epoll create exception";
    using EpollException::EpollException;
    EpollCreateException(int err): EpollException(TAG, err) {}
};

} // fluent
#endif