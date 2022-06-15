#ifndef __FLUENT_THROWS_WRITE_EXCEPTION_H__
#define __FLUENT_THROWS_WRITE_EXCEPTION_H__
#include "IoException.h"
namespace fluent {

class WriteException: public IoException {
public:
    static constexpr const char *TAG = "write exception";
    using IoException::IoException;
    WriteException(int err): IoException(TAG, err) {}
};

} // fluent
#endif