#ifndef __FLUENT_THROWS_FLUENT_EXCEPTION_H__
#define __FLUENT_THROWS_FLUENT_EXCEPTION_H__
#include <bits/stdc++.h>
namespace fluent {

class FluentException: public std::exception {
private:
    const char *_info;
public:
    explicit FluentException(const char *info)
        : _info(info) {}
    const char* what() const noexcept override
        { return _info; }
};

} // fluent
#endif