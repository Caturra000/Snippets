#ifndef __FLUENT_FUTURE_FUTURE_H__
#define __FLUENT_FUTURE_FUTURE_H__
#include <bits/stdc++.h>
#include "FunctionTraits.h"
#include "FutureTraits.h"
#include "Looper.h"
#include "ControlBlock.h"
#include "Promise.h"
namespace fluent {

template <typename T>
class Promise;

template <typename T>
class Future {
public:
    Future(Looper *looper,
           const SharedPtr<ControlBlock<T>> &shared)
        : _looper(looper),
          _shared(shared) {}

    Future(const Future &) = delete;
    Future(Future &&) = default;
    Future& operator=(const Future&) = delete;
    Future& operator=(Future&&) = default;

    // TIMEOUT?
    bool hasResult() { return _shared->_state == State::READY; }

    // unsafe
    // ensure: has result
    T get() {
        auto value = std::move(_shared->_value);
        _shared->_state = State::DEAD;
        return value;
    }

    // unsafe
    SharedPtr<ControlBlock<T>> getControlBlock() {
        return _shared;
    }

    // must receive functor R(T) R(T&) R(T&&)
    template <typename Functor,
              typename R = typename FunctionTraits<Functor>::ReturnType,
              typename Check = typename std::enable_if<
                    IsThenValid<Future<T>, Functor>::value>::type>
    Future<R> then(Functor &&f) {
        // T or T& or T&& ?
        // using ForwardType = decltype(std::get<0>(std::declval<typename FunctionTraits<Functor>::ArgsTuple>()));
        using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
        Promise<R> promise(_looper);
        auto future = promise.get();
        State state = _shared->_state;
        if(state == State::NEW || state == State::READY) {
            // async request, will be set value and then callback
            setCallback([f = std::forward<Functor>(f), promise = std::move(promise)](T &&value) mutable {
                // for callback f
                // f(T) and f(T&) will copy current Future<T> value in shared,
                // - T will copy to your routine. It is safe to move
                // - T& just use reference where it is in shared state
                // T&& will move the parent future value
                // - T&& just use reference, but moving this value to your routine is unsafe
                promise.setValue(f(std::forward<ForwardType>(value)));
            });
            if(state == State::READY) {
                postRequest();
            }
        } else if(state == State::CANCEL) {
            // return a future will never be setValue()
            promise.cancel();
        }
        return future;
    }

    // receive: bool(T) bool(T&) bool(T&&)
    // note: bool(T&&) may be unsafe (will move last future result)
    // return: Future<T>
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
            /*bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,*/
              bool ShouldReturnBool = std::is_same<typename FunctionTraits<Functor>::ReturnType, bool>::value,
              typename PollRequired = typename std::enable_if<AtLeastThenValid /*&& WontAcceptRvalue*/ && ShouldReturnBool>::type>
    Future<T> poll(Functor &&f) {
        using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
        Promise<T> promise(_looper);
        auto future = promise.get();
        State state = _shared->_state;
        if(state == State::NEW || state == State::READY) {
            // reuse _then
            setCallback([f = std::forward<Functor>(f), promise = std::move(promise), looper = _looper](T &&value) mutable {
                if(f(std::forward<ForwardType>(value))) {
                    promise.setValue(std::forward<ForwardType>(value));
                } else {
                    looper->yield();
                }
            });
            if(state == State::READY) {
                postRequest();
            }
        } else if(_shared->_state == State::CANCEL) {
            // return a future will never be setValue()
            promise.cancel();
        }
        return future;
    }

    // receive: bool(T) bool(T&) bool(T&&)
    // note: bool(T&&) may be unsafe (will move last future result)
    // return: Future<T>
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
            /*bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,*/
              bool ShouldReturnBool = std::is_same<typename FunctionTraits<Functor>::ReturnType, bool>::value,
              typename PollRequired = typename std::enable_if<AtLeastThenValid /*&& WontAcceptRvalue*/ && ShouldReturnBool>::type>
    Future<T> poll(size_t count, Functor &&f) {
        using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
        Promise<T> promise(_looper);
        auto future = promise.get();
        State state = _shared->_state;
        if(state == State::NEW || state == State::READY) {
            setCallback([f = std::forward<Functor>(f), promise = std::move(promise), looper = _looper, count](T &&value) mutable {
                if(count-- == 0 || f(std::forward<ForwardType>(value))) {
                    promise.setValue(std::forward<ForwardType>(value));
                } else {
                    looper->yield();
                }
            });
            if(state == State::READY) {
                postRequest();
            }
        } else if(_shared->_state == State::CANCEL) {
            promise.cancel();
        }
        return future;
    }

    // receive: bool(T) bool(T&) bool(T&&)
    // return: Future<T>
    // note: bool(T&&) may be unsafe (will move last future result)
    // IMPROVEMENT: return future<T>& ?
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
            /*bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,*/
              bool ShouldReturnBool = std::is_same<typename FunctionTraits<Functor>::ReturnType, bool>::value,
              typename CancelIfRequired = typename std::enable_if<AtLeastThenValid /*&& WontAcceptRvalue*/ && ShouldReturnBool>::type>
    Future<T> cancelIf(Functor &&f) {
        using ForwardType = typename std::tuple_element<0, typename FunctionTraits<Functor>::ArgsTuple>::type;
        Promise<T> promise(_looper);
        auto future = promise.get();
        State state = _shared->_state;
        if(state == State::NEW || state == State::READY) {
            setCallback([f = std::forward<Functor>(f), promise = std::move(promise)](T &&value) mutable {
                if(f(std::forward<ForwardType>(value))) {
                    promise.cancel();
                } else {
                    // forward the current future to the next
                    promise.setValue(std::forward<ForwardType>(value));
                }
            });
            if(state == State::READY) {
                postRequest();
            }
        } else if(_shared->_state == State::CANCEL) {
            promise.cancel();
        }
        return future;
    }

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
              bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,
              bool ShouldReturnVoid = std::is_same<typename FunctionTraits<Functor>::ReturnType, void>::value,
              typename WaitRequired = typename std::enable_if<AtLeastThenValid && WontAcceptRvalue && ShouldReturnVoid>::type>
    Future<T> wait(size_t count, std::chrono::milliseconds duration, Functor &&f) {
        auto start = std::chrono::system_clock::time_point{};
        return poll([f = std::forward<Functor>(f), start, duration, remain = count](T &self) mutable {
            auto current = std::chrono::system_clock::now();
            // call once
            if(start == std::chrono::system_clock::time_point{}) {
                // unlikely
                if(remain == 0) {
                    return true;
                }
                start = current;
            }
            if(current - start >= duration) {
                f(self);
                if(--remain == 0) {
                    return true;
                }
                start = /*current*/ std::chrono::system_clock::now();
            }
            return false;
        });
    }

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
              bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,
              bool ShouldReturnVoid = std::is_same<typename FunctionTraits<Functor>::ReturnType, void>::value,
              typename WaitRequired = typename std::enable_if<AtLeastThenValid && WontAcceptRvalue && ShouldReturnVoid>::type>
    Future<T> wait(std::chrono::milliseconds duration, Functor &&f) {
        return wait(1, duration, std::forward<Functor>(f));
    }

    // receive: void(T) void(T&)
    // return: Future<T>
    template <typename Functor,
              bool AtLeastThenValid = IsThenValid<Future<T>, Functor>::value,
              bool WontAcceptRvalue = !std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<T&&>>::value,
              bool ShouldReturnVoid = std::is_same<typename FunctionTraits<Functor>::ReturnType, void>::value,
              typename WaitRequired = typename std::enable_if<AtLeastThenValid && WontAcceptRvalue && ShouldReturnVoid>::type>
    Future<T> wait(std::chrono::system_clock::time_point timePoint, Functor &&f) {
        return poll([f = std::forward<Functor>(f), timePoint](T &self) mutable {
            auto current = std::chrono::system_clock::now();
            if(current >= timePoint) {
                f(self);
                return true;
            }
            return false;
        });
    }

private:
    void setCallback(const std::function<void(T&&)> &f) {
        _shared->_then = f;
    }

    void setCallback(std::function<void(T&&)> &&f) {
        _shared->_then = std::move(f);
    }

    // unsafe
    // ensure: READY & has then_
    void postRequest() {
        _shared->_state = State::POSTED;
        _looper->post([shared = _shared] {
            // shared->_value may be moved
            // _then must be T&&
            shared->_then(static_cast<T&&>(shared->_value));
            shared->_state = State::DONE;
        });
    }

private:
    Looper                           *_looper;
    SharedPtr<ControlBlock<T>> _shared;
};

} // fluent
#endif