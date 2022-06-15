#ifndef __FLUENT_NETWORK_POOL_H__
#define __FLUENT_NETWORK_POOL_H__
#include <bits/stdc++.h>
#include "../utils/WeakReference.h"
#include "Multiplexer.h"
#include "Context.h"
namespace fluent {

class Pool: private WeakReference<Pool> {
public:
    // unique_ptr makes Context pointer safe in vector rescale and std::swap
    // token can be accessible in global(server group) multiplexer
    using BundleValueType = std::pair<Multiplexer::Token, Context>;
    using Container = std::vector<std::unique_ptr<BundleValueType>>;

    constexpr static size_t GC_THRESHOLD = 16;
    constexpr static size_t PREFETCH = 16;

public:
    // return a bundle for two phase construction
    template <typename ...ContextArgs>
    Multiplexer::Bundle emplace(Multiplexer::Token token, ContextArgs &&...args);

private:
    Container _container {PREFETCH};

// CRTP interface
private:
    using GcBase = WeakReference<Pool>;
    friend class WeakReference<Pool>;

    // global
    bool isReusable();
    // connection
    bool isReusable(size_t index);
    // access
    Container::reference get(size_t index);
};

template <typename ...ContextArgs>
inline Multiplexer::Bundle Pool::emplace(Multiplexer::Token token, ContextArgs &&...args) {
    if(_container.size() > GC_THRESHOLD) GcBase::updateReusableIndex();
    auto connection = std::make_unique<BundleValueType>(token, Context(std::forward<ContextArgs>(args)...));
    if(isReusable() && isReusable(GcBase::_reusableIndex)) {
        size_t pos = GcBase::_reusableIndex++;
        _container[pos] = std::move(connection);
        return _container[pos].get();
    }
    _container.emplace_back(std::move(connection));
    return _container.back().get();
}

inline bool Pool::isReusable() {
    return GcBase::_reusableIndex != _container.size();
}

inline bool Pool::isReusable(size_t index) {
    auto &connection = _container[index];
    if(connection == nullptr) return true;
    auto &context = connection->second;
    if(!context.isDisConnected()) return false;
    const StrongLifecycle& lifecycle = context.getLifecycle();
    // lifecycle disabled or no strong guard
    return lifecycle == nullptr || lifecycle.use_count() <= 1;
}

inline Pool::Container::reference Pool::get(size_t index) {
    return _container[index];
}

} // fluent
#endif