#ifndef __FLUENT_UTILS_NON_COPYABLE_H__
#define __FLUENT_UTILS_NON_COPYABLE_H__
namespace fluent {

class NonCopyable {
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};

} // fluent
#endif