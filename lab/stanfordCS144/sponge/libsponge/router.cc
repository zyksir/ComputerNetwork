#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    // cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
    //      << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num <<
    //      "\n";

    const auto key = make_pair(prefix_length, route_prefix);
    const auto value = make_pair(interface_num, next_hop);
    _forwarding_table.emplace(key, value);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    static const uint32_t MASK = 0xffffffff;
    using kv_type = decltype(*_forwarding_table.begin());
    static const auto match_function = [](const uint32_t src_ip, const kv_type &key) {
        const auto postfix_length = 32 - key.first.first;
        const auto &route_prefix = key.first.second;
        if (postfix_length == 32) {
            return true;
        } else if (postfix_length == 0) {
            return route_prefix == src_ip;
        }
        const uint32_t mask = MASK << postfix_length;
        return (route_prefix & mask) == (src_ip & mask);
    };
    if (dgram.header().ttl <= 1) {
        return;
    }
    dgram.header().ttl--;
    uint32_t dst = dgram.header().dst;
    const auto this_match_function = [dst](const kv_type &key) { return match_function(dst, key); };
    const auto it = find_if(_forwarding_table.begin(), _forwarding_table.end(), this_match_function);
    if (it == _forwarding_table.end()) {
        return;
    }
    auto [interface_num, next_hop] = it->second;
    auto real_next_hop = next_hop.has_value() ? next_hop.value() : Address::from_ipv4_numeric(dgram.header().dst);
    interface(interface_num).send_datagram(dgram, real_next_hop);
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
