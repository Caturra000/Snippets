#ifndef __FLUENT_THROWS_ERRNO_EXCEPTION_H__
#define __FLUENT_THROWS_ERRNO_EXCEPTION_H__
#include <cstring>
#include "FluentException.h"
namespace fluent {

class ErrnoException: public FluentException {
private:
    int _err {0}; // 暂存errno
    std::string _what;
public:
    static constexpr const char *TAG = "errno exception";
    using FluentException::FluentException;
    ErrnoException(const char *info, int err)
        : FluentException(info), _err(err), _what(std::string(info) + ' ' + strerror(_err)) {}
    ErrnoException(int err): ErrnoException(TAG, err) {}
    int errorCode() { return _err; }
    const char* errorMessage() { return strerror(_err); }
    const char* what() const noexcept override { return _what.c_str(); }
};

} // fluent
#endif