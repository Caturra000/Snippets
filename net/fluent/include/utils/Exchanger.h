#ifndef __FLUENT_UTILS_EXCHANGER_H__
#define __FLUENT_UTILS_EXCHANGER_H__
#include <bits/stdc++.h>
namespace fluent {

class Exchanger final {
public:

    /*constexpr*/ Exchanger(): _content(nullptr) {}
    template <typename ValueType, typename NonRef = typename std::remove_reference<ValueType>::type>
    Exchanger(ValueType &&value): _content(new Holder<NonRef>(static_cast<ValueType&&>(value))) {}
    Exchanger(Exchanger &&rhs): _content(rhs._content) { rhs._content = nullptr; }
    ~Exchanger() { delete _content; }

    Exchanger& swap(Exchanger &rhs) {
        std::swap(_content, rhs._content);
        return *this;
    }

    Exchanger& operator=(Exchanger &&rhs) {
        rhs.swap(*this);
        Exchanger().swap(rhs);
        return *this;
    }

    template <typename ValueType>
    Exchanger& operator=(ValueType &&rhs) {
        Exchanger(std::forward<ValueType>(rhs)).swap(*this);
        return *this;
    }

private:
    class PlaceHolder {
    public:
        virtual ~PlaceHolder() {} //
    };

    template <typename ValueType>
    class Holder: public PlaceHolder {
    public:
        Holder(ValueType &&value): _held(static_cast<ValueType&&>(value)) {}
        ValueType _held;

        Holder& operator=(const Holder&) = delete;
    };

private:
    template <typename ValueType>
    friend ValueType* cast(Exchanger*);

    PlaceHolder* _content;
};

inline void swap(Exchanger &lhs, Exchanger &rhs) {
    lhs.swap(rhs);
}

template <typename ValueType>
inline ValueType* cast(Exchanger *object) {
    return object ?
        std::addressof(static_cast<Exchanger::Holder<ValueType>*>(object->_content)->_held)
        : nullptr;
}

template <typename ValueType>
inline const ValueType* cast(const Exchanger *object) {
    return cast<ValueType>(const_cast<Exchanger*>(object));
}


// &
template <typename ValueType>
inline ValueType cast(Exchanger &object) {
    using NonRef = typename std::remove_reference<ValueType>::type;
    return *(cast<NonRef>(std::addressof(object)));
}

// &
template <typename ValueType>
inline ValueType cast(const Exchanger &object) {
    using NonRef = typename std::remove_reference<ValueType>::type;
    return cast<const NonRef &>(const_cast<Exchanger&>(object));
}

} // fluent
#endif