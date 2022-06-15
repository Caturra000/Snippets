#ifndef __FLUENT_UTILS_OBJECT_H__
#define __FLUENT_UTILS_OBJECT_H__
#include <bits/stdc++.h>
namespace fluent {

class Object final {
public:

    /*constexpr*/ Object(): _content(nullptr) {}
    template <typename ValueType, typename NonRef = typename std::remove_reference<ValueType>::type>
    Object(ValueType &&value): _content(new Holder<NonRef>(std::forward<ValueType>(value))) {}
    Object(const Object &rhs): _content(rhs._content ? rhs._content->clone() : nullptr) {}
    Object(Object &&rhs): _content(rhs._content) { rhs._content = nullptr; }
    ~Object() { delete _content; }

    Object& swap(Object &rhs) {
        std::swap(_content, rhs._content);
        return *this;
    }

    Object& operator=(const Object &rhs) {
        Object(rhs).swap(*this);
        return *this;
    }

    Object& operator=(Object &&rhs) {
        rhs.swap(*this);
        Object().swap(rhs);
        return *this;
    }

    template <typename ValueType>
    Object& operator=(ValueType &&rhs) {
        Object(std::forward<ValueType>(rhs)).swap(*this);
        return *this;
    }

private:
    class PlaceHolder {
    public:
        virtual ~PlaceHolder() {} //
        virtual PlaceHolder* clone() const = 0;
    };

    template <typename ValueType>
    class Holder: public PlaceHolder {
    public:
        Holder(const ValueType &value): _held(value) {}
        Holder(ValueType &&value): _held(static_cast<ValueType&&>(value)) {}
        PlaceHolder* clone() const override { return new Holder(_held); }
        ValueType _held;

        Holder& operator=(const Holder&) = delete;
    };

private:
    template <typename ValueType>
    friend ValueType* cast(Object*);

    PlaceHolder* _content;
};

inline void swap(Object &lhs, Object &rhs) {
    lhs.swap(rhs);
}

template <typename ValueType>
inline ValueType* cast(Object *object) {
    return object ?
        std::addressof(static_cast<Object::Holder<ValueType>*>(object->_content)->_held)
        : nullptr;
}

template <typename ValueType>
inline const ValueType* cast(const Object *object) {
    return cast<ValueType>(const_cast<Object*>(object));
}

template <typename ValueType>
inline ValueType cast(Object &object) {
    using NonRef = typename std::remove_reference<ValueType>::type;
    return *(cast<NonRef>(std::addressof(object)));
}

template <typename ValueType>
inline ValueType cast(const Object &object) {
    using NonRef = typename std::remove_reference<ValueType>::type;
    return cast<const NonRef &>(const_cast<Object&>(object));
}

} // fluent
#endif