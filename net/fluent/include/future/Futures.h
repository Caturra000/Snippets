#ifndef __FLUENT_FUTURE_FUTURES_H__
#define __FLUENT_FUTURE_FUTURES_H__
#include <bits/stdc++.h>
#include "Future.h"
#include "Promise.h"
#include "FuturesInternal.h"
namespace fluent {

template <typename ...Args, typename Tuple = std::tuple<std::decay_t<Args>...>>
inline Future<Tuple> makeTupleFuture(Looper *looper, Args &&...args) {
    Promise<Tuple> promise(looper);
    promise.setValue(std::make_tuple(std::forward<Args>(args)...));
    return promise.get();
}

template <typename T, typename R = std::decay_t<T>>
inline Future<R> makeFuture(Looper *looper, T &&arg) {
    Promise<R> promise(looper);
    promise.setValue(std::forward<T>(arg));
    return promise.get();
}

// whenAll return copied values
// futures must be lvalue
// futs: Future<T1>, Future<T2>, Future<T3>...
// return: Future<std::tuple<T1, T2, T3...>>
template <typename ...Futs>
inline auto whenAll(Looper *looper, Futs &...futs) {
    static_assert(sizeof...(futs) > 0, "whenAll should receive future");
    using ResultTuple = std::tuple<typename FutureInner<Futs>::Type...>;
    return details::whenAllTemplate<ResultTuple>(looper, std::make_tuple(futs.getControlBlock()...));
}

// futs : Future<T>, Future<T>, Future<T>...
// return: Future<std::vector<std::pair<size_t, T>>>
// returns index and result
template <typename Fut, typename ...Futs>
inline auto whenN(size_t n, Looper *looper, Fut &fut, Futs &...futs) {
    // assert(N <= 1 + sizeof...(futs));
    using T = typename FutureInner<Fut>::Type;
    using ControlBlockType = SharedPtr<ControlBlock<T>>;
    using QueryPair = std::pair<size_t, ControlBlockType>;
    using ResultPair = std::pair<size_t, T>; // index and result
    using QueryVector = std::vector<QueryPair>;
    using ResultVector = std::vector<ResultPair>;
    QueryVector queries;

    // collect
    {
        std::vector<ControlBlockType> controlBlocks {
            fut.getControlBlock(), futs.getControlBlock()...
        };
        for(size_t index = 0, N = controlBlocks.size(); index != N; ++index) {
            queries.emplace_back(index, std::move(controlBlocks[index]));
        }
    };

    return details::whenNTemplate<T, ResultVector>(n, looper, std::move(queries), [](const T &) { return true; });
}

template <typename Iterator>
inline auto whenN(size_t n, Looper *looper, Iterator first, Iterator last) {
    // assert(N <= 1 + sizeof...(futs));
    using Fut = typename Iterator::value_type;
    using T = typename FutureInner<Fut>::Type;
    using ControlBlockType = SharedPtr<ControlBlock<T>>;
    using QueryPair = std::pair<size_t, ControlBlockType>;
    using ResultPair = std::pair<size_t, T>;
    using QueryVector = std::vector<QueryPair>;
    using ResultVector = std::vector<ResultPair>;
    QueryVector queries;

    size_t index = 0;
    for(auto cur = first; cur != last; cur = std::next(cur)) {
        queries.emplace_back(index, cur->getControlBlock());
        index++;
    }

    return details::whenNTemplate<T, ResultVector>(n, looper, std::move(queries), [](const T &) { return true; });
}

template <typename Iterator, typename Condition>
inline auto whenNIf(size_t n, Looper *looper, Iterator first, Iterator last, Condition &&cond) {
    // assert(N <= 1 + sizeof...(futs));
    using Fut = typename Iterator::value_type;
    using T = typename FutureInner<Fut>::Type;
    using ControlBlockType = SharedPtr<ControlBlock<T>>;
    using QueryPair = std::pair<size_t, ControlBlockType>;
    using ResultPair = std::pair<size_t, T>;
    using QueryVector = std::vector<QueryPair>;
    using ResultVector = std::vector<ResultPair>;
    QueryVector queries;

    size_t index = 0;
    for(auto cur = first; cur != last; cur = std::next(cur)) {
        queries.emplace_back(index, cur->getControlBlock());
        index++;
    }

    return details::whenNTemplate<T, ResultVector>(n, looper, std::move(queries), std::forward<Condition>(cond));
}

// futs : Future<T>, Future<T>, Future<T>...
// return: Future<std::pair<size_t, T>>
// returns index and result
template <typename Fut, typename ...Futs>
inline auto whenAny(Looper *looper, Fut &fut, Futs &...futs) {
    using T = typename FutureInner<Fut>::Type;
    return whenN(1, looper, fut, futs...)
        .then([](std::vector<std::pair<size_t, T>> &&results) {
            auto result = std::move(results[0]);
            return result;
        });
}

template <typename Iterator>
inline auto whenAny(Looper *looper, Iterator first, Iterator last) {
    using Fut = typename Iterator::value_type;
    using T = typename FutureInner<Fut>::Type;
    return whenN(1, looper, first, last)
        .then([](std::vector<std::pair<size_t, T>> &&results) {
            auto result = std::move(results[0]);
            return result;
        });
}

template <typename Iterator, typename Condition>
inline auto whenAnyIf(Looper *looper, Iterator first, Iterator last, Condition &&cond) {
    using Fut = typename Iterator::value_type;
    using T = typename FutureInner<Fut>::Type;
    return whenNIf(1, looper, first, last, std::forward<Condition>(cond))
        .then([](std::vector<std::pair<size_t, T>> &&results) {
            auto result = std::move(results[0]);
            return result;
        });
}

} // fluent
#endif