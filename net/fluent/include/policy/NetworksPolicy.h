#ifndef __FLUENT_POLICY_NETWORKS_POLICY_H__
#define __FLUENT_POLICY_NETWORKS_POLICY_H__
namespace fluent {

class NetworksPolicy {
public:
enum class NetworkState {
        CONNECTING,
        CONNECTED,
        DISCONNECTING,
        DISCONNECTED,
    };

    bool isConnecting() const { return _nState == NetworkState::CONNECTING; }
    bool isConnected() const { return _nState == NetworkState::CONNECTED; }
    bool isDisConnecting() const { return _nState == NetworkState::DISCONNECTING; }
    bool isDisConnected() const { return _nState == NetworkState::DISCONNECTED; }

protected:
    NetworkState _nState {NetworkState::CONNECTING};

// log helper
public:
    const char* networkInfo() const;
};

inline const char* NetworksPolicy::networkInfo() const {
    switch(_nState) {
        case NetworkState::CONNECTING: return "CONNECTING";
        case NetworkState::CONNECTED: return "CONNECTED";
        case NetworkState::DISCONNECTING: return "DISCONNECTING";
        case NetworkState::DISCONNECTED: return "DISCONNECTED";
        default: return "?";
    }
}

} // fluent
#endif