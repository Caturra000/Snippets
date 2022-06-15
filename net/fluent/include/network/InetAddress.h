#ifndef __FLUENT_NETWORK_INET_ADDRESS_H__
#define __FLUENT_NETWORK_INET_ADDRESS_H__
#include <unistd.h>
#include <netinet/in.h>
#include <string>
#include "../utils/StringUtils.h"
namespace fluent {

struct InetAddress {
public:
    InetAddress() = default;
    InetAddress(sockaddr_in address): _address(address) { createReport(); }
    InetAddress(uint32_t ip, uint16_t port): _address{AF_INET, ::htons(port), ::htonl(ip)} { createReport(); }
    InetAddress(const std::string &ip, uint16_t port): InetAddress(stringToIp(ip), port) { createReport(); }
    InetAddress(const std::string &address);

    std::string toString() const { return ipToString() + ":" + std::to_string(::ntohs(_address.sin_port)); }
    std::string toStringPretty() const { return '(' + toString() + ')'; }
    uint16_t rawPort() const { return ::ntohs(_address.sin_port); };
    uint32_t rawIp() const { return ::ntohl(_address.sin_addr.s_addr); };

private:
    std::string ipToString() const;
    uint32_t stringToIp(std::string s) const; // static
    void createReport() {} // debug

    sockaddr_in _address;
};

inline InetAddress::InetAddress(const std::string &address) {
    auto pivot = split(address, ':');
    assert(pivot.size() == 2);
    _address = {
        AF_INET,
        ::htons(toDec<uint16_t>(address.substr(pivot[1].first, pivot[1].second - pivot[1].first))),
        ::htonl(stringToIp(address.substr(pivot[0].first, pivot[0].second - pivot[0].first)))
    };
}

inline std::string InetAddress::ipToString() const {
    std::string s;
    uint32_t addr = ::ntohl(_address.sin_addr.s_addr);
    for(int chunk = 3; ~chunk; --chunk) {
        s.append(".", 3 > chunk).append(std::to_string((addr >> (chunk << 3)) & 0xff));
    }
    return s;
}

inline uint32_t InetAddress::stringToIp(std::string s) const {
    auto pivots = split(s, '.');
    uint32_t ip = 0;
    for(auto &&p : pivots) {
        ip = (ip << 8) | toDec<uint32_t>(s.substr(p.first, p.second-p.first));
    }
    return ip;
}

} // fluent

#endif