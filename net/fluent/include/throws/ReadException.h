#ifndef __FLUENT_THROWS_READ_EXCEPTION_H__
#define __FLUENT_THROWS_READ_EXCEPTION_H__
#include "IoException.h"
namespace fluent {

class ReadException: public IoException {
public:
    static constexpr const char *TAG = "read exception";
    using IoException::IoException;
    ReadException(int err): IoException(TAG, err) {}
};

} // fluent
#endif