#ifndef __FLUENT_UTILS_WEAK_REFERENCE_H__
#define __FLUENT_UTILS_WEAK_REFERENCE_H__
#include <bits/stdc++.h>
namespace fluent {

// GC interface with CRTP style
// should implement isReusable() / get()
template <typename T, size_t step = 2>
class WeakReference {
public:
    WeakReference(): _reusableIndex(0), _window{0, -1} {}

    void updateReusableIndex();

public:
    ssize_t _reusableIndex;
    struct { ssize_t left, right; } _window;

private:
    void resetWindow();
    void doUpdate();
};

template <typename T, size_t step>
inline void WeakReference<T, step>::updateReusableIndex() {
    for(size_t _ = 0; _ < step; ++_) {
        if(_reusableIndex != 0) {
            doUpdate();
        } else {
            break;
        }
    }
}

template <typename T, size_t step>
inline void WeakReference<T, step>::resetWindow() {
    _window.left = 0;
    _window.right = -1;
}

template <typename T, size_t step>
inline void WeakReference<T, step>::doUpdate() {
    // closed window
    if(_window.left > _window.right) {
        if(static_cast<T*>(this)->isReusable(_window.left)) {
            // open window
            ++_window.right;
        } else {
            ++_window.left;
            ++_window.right;
            // would be invalid
            // reset to initial position
            if(_window.left == _reusableIndex) {
                resetWindow();
            }
        }
        return;
    }

    // actived window
    //  left < right

    // ensure right < _reusableIndex
    if(_window.right + 1 == _reusableIndex) {
        // merge the window
        // finally get a better index
        // [left = new_reusable_idex, right] + [old_reusable_index, UNKNOWN_BOUND)
        // ==> [new_reusable_index, UNKNOWN_BOUND)
        _reusableIndex = _window.left;
        resetWindow();

    // _window.right + 1 < _reusableIndex

    } else if(static_cast<T*>(this)->isReusable(_window.right + 1)) {
        ++_window.right;
    } else {
        // _window.right + 1 is not reusable
        // but we can swap the left resuable resource and go on
        std::swap(static_cast<T*>(this)->get(++_window.right),
                    static_cast<T*>(this)->get(_window.left++));
    }
}

} // fluent
#endif