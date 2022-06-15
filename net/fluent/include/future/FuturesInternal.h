#ifndef __FLUENT_FUTURE_FUTURES_INTERNAL_H__
#define __FLUENT_FUTURE_FUTURES_INTERNAL_H__
#include "bits/stdc++.h"
#include "Future.h"
#include "Promise.h"
namespace fluent {
namespace details {

template <typename Tuple>
struct TupleHelper {
public:
    using DecayTuple = std::decay_t<Tuple>;

    template <typename Functor>
    static void forEach(DecayTuple &tup, Functor &&f) {
        test<0>(tup, /*std::forward<Functor>*/(f));
    }

private:
    template <size_t I, typename Functor>
    static std::enable_if_t<(I+1 < std::tuple_size<DecayTuple>::value)>
    test(DecayTuple &tup, Functor &&f) {
        // true : continue
        if(f(std::get<I>(tup))) {
            test<I+1>(tup, std::forward<Functor>(f));
        }
    }

    // last
    template <size_t I, typename Functor>
    static std::enable_if_t<(I+1 == std::tuple_size<DecayTuple>::value)>
    test(DecayTuple &tup, Functor &&f) {
        f(std::get<I>(tup));
    }
};

template <size_t ...Is>
struct TupleHelper<std::index_sequence<Is...>> {
    template <typename ControlBlockTuple>
    static auto makeResultTuple(ControlBlockTuple &&tup) {
        // decay to lvalue
        return std::make_tuple(std::get<Is>(tup)->_value...);
    }
};

template <typename ResultTuple, typename ControlBlockTupleForward>
inline auto whenAllTemplate(Looper *looper, ControlBlockTupleForward &&controlBlocks) {
    using ControlBlockTuple = typename std::remove_reference<ControlBlockTupleForward>::type;
    using Binder = std::tuple<ControlBlockTuple, ResultTuple>;
    return makeTupleFuture(looper,
                           std::forward<ControlBlockTupleForward>(controlBlocks),
                           ResultTuple{})
        // at most X times?
        .poll(/*X, */[](Binder &binder) {
            bool pass = true;
            auto &cbs = std::get<0>(binder);
            auto &res = std::get<1>(binder);
            TupleHelper<ControlBlockTuple>::forEach(cbs, [&pass](auto &&elem) mutable {
                // IMPROVEMENT: O(n) algorithm
                // or divide-and-conqure by yourself
                // note: cannot cache pass result for the next poll, because state will die or cancel
                auto state = elem->_state;
                bool hasValue = (state == State::READY) || (state == State::POSTED) || (state == State::DONE);
                return pass &= hasValue;
            });
            // lock values here if true
            if(pass) {
                using Sequence = typename std::make_index_sequence<std::tuple_size<ResultTuple>::value>;
                res = TupleHelper<Sequence>::makeResultTuple(cbs);
                // now it's safe to then()
            }
            return pass;
        })
        .then([](Binder &&binder) {
            auto res = std::move(std::get<1>(binder));
            return res;
        });
}


template <typename T, typename ResultVector, typename QueryVectorForward, typename Functor,
          bool AcceptConstReference = std::is_same<typename FunctionTraits<Functor>::ArgsTuple, std::tuple<const T&>>::value,
          bool ShouldReturnBool = std::is_same<typename FunctionTraits<Functor>::ReturnType, bool>::value,
          typename WhenNRequired = typename std::enable_if<AcceptConstReference && ShouldReturnBool>::type>
inline auto whenNTemplate(size_t n, Looper *looper, QueryVectorForward &&queries, Functor &&cond) {
    using QueryVector = typename std::remove_reference<QueryVectorForward>::type;
    using Binder = std::tuple<QueryVector, ResultVector>;
    return makeTupleFuture(looper, std::forward<QueryVectorForward>(queries), ResultVector{})
        .poll([noresponse = QueryVector{}, remain = n, cond = std::forward<Functor>(cond)](Binder &binder) mutable {
            auto &queries = std::get<0>(binder);
            auto &results = std::get<1>(binder);
            while(remain && !queries.empty()) {
                // pair: index + controllBlock
                auto &query = queries.back();
                State state = query.second->_state;
                bool hasValue = (state == State::READY) || (state == State::POSTED) || (state == State::DONE);
                if(hasValue && cond(query.second->_value)) {
                    results.emplace_back(query.first, query.second->_value);
                    remain--;
                } else {
                    noresponse.emplace_back(std::move(query));
                }
                queries.pop_back();
            }
            noresponse.swap(queries);
            return remain == 0;
        })
        .then([](Binder &&binder) {
            auto res = std::move(std::get<1>(binder));
            // sort by index
            std::sort(res.begin(), res.end(), [](auto &&lhs, auto &&rhs) {
                return lhs.first < rhs.first;
            });
            return res;
        });
}

} // details
} // fluent
#endif