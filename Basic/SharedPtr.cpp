#include <bits/stdc++.h>

// 未完全测试
template <typename T>
struct SharedPtr {

    SharedPtr(): ptr(nullptr), refCount(nullptr) {}
    SharedPtr(T *ptr): ptr(ptr), refCount(new size_t(1)) {}
    // 踩了个swap无限循环的坑，以后注意
    SharedPtr(const SharedPtr<T> &sp): ptr(sp.ptr), refCount(sp.refCount) { if(refCount) (*refCount)++; }
    SharedPtr(SharedPtr<T> &&sp): ptr(sp.ptr), refCount(sp.refCount) { sp.ptr = nullptr; sp.refCount = nullptr; }

    void swap(SharedPtr<T> &that) {
        std::swap(ptr, that.ptr);
        std::swap(refCount, that.refCount);
    }

    SharedPtr& operator=(T *ptr) {
        // 写法1
        // if(this->ptr == ptr) return *this;
        // std::swap(*this, that);


        // 写法2
        // SharedPtr<T>{ptr}.swap(*this);

        // 写法3
        this->~SharedPtr();
        this->ptr = ptr;
        if(ptr) this->refCount = new size_t(1);

        return *this;
    }

    SharedPtr& operator=(SharedPtr<T> sp) {
        sp.swap(*this);
        return *this;
    }

    T* operator->() {
        return ptr;
    }

    T& operator*() {
        return *ptr;
    }

    operator bool() {
        return ptr != nullptr;
    }


    ~SharedPtr() {
        if(refCount && !--(*refCount)) {
            delete ptr;
            delete refCount;
        }
    }

    T *ptr;
    size_t *refCount;
};

int main() {
    SharedPtr<std::string> sp(new std::string("abc"));
    sp = new std::string("qaq");
    if(sp) std::cout << *sp << std::endl;
    return 0;
}