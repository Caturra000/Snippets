#ifndef __FLUENT_POLICY_SEND_POLICY_H__
#define __FLUENT_POLICY_SEND_POLICY_H__
#include <bits/stdc++.h>
namespace fluent {

class SendPolicy {
public:
    class Completion {
    public:
        // index flag: never
        constexpr static size_t INVALID = 0;
        // index flag: always
        constexpr static size_t FAST_COMPLETE = 1;
    private:
        size_t _token;
        SendPolicy *_master;
    public:
        bool poll() const { return _master->_sendCompletedIndex > _token; }
        Completion(size_t token, SendPolicy *master)
            : _token(token), _master(master) {}
    };

protected:
    std::queue<size_t> _sendProvider;
    size_t _sendCurrentIndex {Completion::FAST_COMPLETE + 1};
    size_t _sendCompletedIndex {Completion::FAST_COMPLETE + 1};
};

} // fluent
#endif