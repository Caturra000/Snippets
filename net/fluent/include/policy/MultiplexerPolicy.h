#ifndef __FLUENT_POLICY_MULTIPLEXER_POLICY_H__
#define __FLUENT_POLICY_MULTIPLEXER_POLICY_H__
#include <bits/stdc++.h>
namespace fluent {

class Multiplexer;
class Context;

class MultiplexerPolicy {
public:
    using EventBitmap = uint32_t;
    constexpr static EventBitmap EVENT_NONE = 0;
    constexpr static EventBitmap EVENT_READ = POLL_IN | POLL_PRI;
    constexpr static EventBitmap EVENT_WRITE = POLL_OUT;

    enum class EventState {
        NEW,
        ADDED,
        DELETED
    };

    using EpollOperationHint = uint;
    constexpr static EpollOperationHint EPOLL_CTL_NONE = 0;

    // update the event state and return next operation hint for epoll
    EpollOperationHint updateEventState();

    EventBitmap events() const { return _events; }
    bool readEventEnabled() const { return _events & EVENT_READ; }
    bool writeEventEnabled() const { return _events & EVENT_WRITE; }

    // note: must update state for epoll
    bool enableRead();
    bool enableWrite();
    bool disableRead();
    bool disableWrite();

protected:
    EventBitmap _events {EVENT_NONE};
    EventState _eState {EventState::NEW};

    constexpr static void checkHardCode();

    // ugly...
    Multiplexer *_multiplexer;
    std::pair<size_t, Context> *_bundle;

// log helper
public:
    char eventStateInfo() const;
    char eventsInfo() const;
};

inline MultiplexerPolicy::EpollOperationHint MultiplexerPolicy::updateEventState() {
    if(_eState == EventState::NEW || _eState == EventState::DELETED) {
        _eState = EventState::ADDED;
        return EPOLL_CTL_ADD;
    }

    // else : EventState::ADD

    if(_events == EVENT_NONE) {
        _eState = EventState::DELETED;
        return EPOLL_CTL_DEL;
    } else {
        return EPOLL_CTL_MOD;
    }
}

inline bool MultiplexerPolicy::enableRead() {
    if(!(_events & EVENT_READ)) {
        _events |= EVENT_READ;
        return true;
    }
    return false;
}

inline bool MultiplexerPolicy::enableWrite() {
    if(!(_events & EVENT_WRITE)) {
        _events |= EVENT_WRITE;
        return true;
    }
    return false;
}

inline bool MultiplexerPolicy::disableRead() {
    if(_events & EVENT_READ) {
        _events &= ~EVENT_READ;
        return true;
    }
    return false;
}

inline bool MultiplexerPolicy::disableWrite() {
    if(_events & EVENT_WRITE) {
        _events &= ~EVENT_WRITE;
        return true;
    }
    return false;
}

inline constexpr void MultiplexerPolicy::checkHardCode() {
    static_assert(EPOLL_CTL_NONE != EPOLL_CTL_ADD
                    && EPOLL_CTL_NONE != EPOLL_CTL_DEL
                    && EPOLL_CTL_NONE != EPOLL_CTL_MOD,
                    "check EPOLL_CTL_NONE");
}

inline char MultiplexerPolicy::eventStateInfo() const {
    switch(_eState) {
        case EventState::NEW: return 'N';
        case EventState::ADDED: return 'A';
        case EventState::DELETED: return 'D';
        default : return '?';
    }
}

inline char MultiplexerPolicy::eventsInfo() const {
    switch(_events) {
        case EVENT_NONE: return 'N';
        case EVENT_READ: return 'R';
        case EVENT_WRITE: return 'W';
        case EVENT_READ | EVENT_WRITE: return 'M'; // Mixed
        default : return '?';
    }
}

} // fluent
#endif