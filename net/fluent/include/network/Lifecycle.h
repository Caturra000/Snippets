#ifndef __FLUENT_NETWORK_LIFECYCLE_H__
#define __FLUENT_NETWORK_LIFECYCLE_H__
#include <bits/stdc++.h>
namespace fluent {

// just a tag class
// class Lifecycle: public std::enable_shared_from_this<Lifecycle> {};

class Lifecycle {};

using StrongLifecycle = std::shared_ptr<Lifecycle>;
using WeakLifecycle = std::weak_ptr<Lifecycle>;

class EnableLifecycle {};

} // fluent
#endif