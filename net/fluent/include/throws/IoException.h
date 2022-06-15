#ifndef __FLUENT_THROWS_IO_EXCEPTION_H__
#define __FLUENT_THROWS_IO_EXCEPTION_H__
#include "ErrnoException.h"
namespace fluent {

class IoException: public ErrnoException {
    using ErrnoException::ErrnoException;
};

} // fluent
#endif