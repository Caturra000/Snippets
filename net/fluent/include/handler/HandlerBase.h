#ifndef __FLUENT_HANDLER_HADLER_BASE_H__
#define __FLUENT_HANDLER_HADLER_BASE_H__
#include <poll.h>
#include <bits/stdc++.h>
#include "HandlerRequire.h"
namespace fluent {

template <typename CRTP, typename Token, typename Bundle>
class HandlerBase {
public:
    constexpr HandlerBase() noexcept { __require(); }

    void handleEvent(Bundle bundle, uint32_t revent) {
        if((revent & POLLHUP) && !(revent & POLLIN)) {
            static_cast<CRTP*>(this)->handleClose(bundle);
        }
        if(revent & (POLLERR | POLLNVAL)) {
            static_cast<CRTP*>(this)->handleError(bundle);
        }
        if(revent & (POLLIN | POLLPRI | POLLRDHUP)) {
            static_cast<CRTP*>(this)->handleRead(bundle);
        }
        if(revent & POLLOUT) {
            static_cast<CRTP*>(this)->handleWrite(bundle);
        }
    }

private:
    constexpr void __require() {
        HandlerRequire<void(Bundle)>::define(&CRTP::handleRead);
        HandlerRequire<void(Bundle)>::define(&CRTP::handleWrite);
        HandlerRequire<void(Bundle)>::define(&CRTP::handleError);
        HandlerRequire<void(Bundle)>::define(&CRTP::handleClose);
    }
};

} // fluent
#endif
