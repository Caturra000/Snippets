#ifndef __FLUENT_POLICY_LIFECYCLE_POLICY_H__
#define __FLUENT_POLICY_LIFECYCLE_POLICY_H__
#include <bits/stdc++.h>
#include "../network/Lifecycle.h"
namespace fluent {

class LifecyclePolicy {
public:
    LifecyclePolicy() = default;
    LifecyclePolicy(EnableLifecycle)
        : _lifecycle(std::make_shared<Lifecycle>()) {}

public:
    // lifecycle is null by default
    // user should explicitly enable this feature
    void ensureLifecycle();

    // opened for user in special case (guarded by const)
    const StrongLifecycle& getLifecycle() { return _lifecycle; }

protected:
    // make async safe, nullptr by default
    StrongLifecycle _lifecycle;
};

inline void LifecyclePolicy::ensureLifecycle() {
    if(!_lifecycle) {
        _lifecycle = std::make_shared<Lifecycle>();
    }
}

} // fluent
#endif