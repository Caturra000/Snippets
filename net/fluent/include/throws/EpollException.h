#ifndef __FLUENT_THROWS_EPOLL_EXCEPTION_H__
#define __FLUENT_THROWS_EPOLL_EXCEPTION_H__
#include "ErrnoException.h"
namespace fluent {

class EpollException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // fluent
#endif