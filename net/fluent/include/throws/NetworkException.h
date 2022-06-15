#ifndef __FLUENT_THROWS_NETWORK_EXCEPTION_H__
#define __FLUENT_THROWS_NETWORK_EXCEPTION_H__
#include "ErrnoException.h"
namespace fluent {

class NetworkException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // fluent
#endif