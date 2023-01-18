#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    // cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
    //      << ip_address.ip() << "\n";
}

void NetworkInterface::send_arp_message(const uint32_t target_ip_address,
                                        const EthernetAddress &target_ethernet_address,
                                        const uint16_t opcode) {
    ARPMessage arp_message;
    arp_message.sender_ethernet_address = _ethernet_address;
    arp_message.sender_ip_address = _ip_address.ipv4_numeric();
    arp_message.target_ip_address = target_ip_address;
    if (opcode == ARPMessage::OPCODE_REPLY) {
        arp_message.target_ethernet_address = target_ethernet_address;
    }
    arp_message.opcode = opcode;

    EthernetFrame arp_frame;
    arp_frame.payload() = arp_message.serialize();
    arp_frame.header().type = EthernetHeader::TYPE_ARP;
    arp_frame.header().src = _ethernet_address;
    arp_frame.header().dst = target_ethernet_address;
    _frames_out.push(arp_frame);

    // don't sent another arp request for this ip in five seconds
}

void NetworkInterface::send_ip_dgram(const InternetDatagram &dgram,
                                     //   const uint32_t target_ip_address,
                                     const EthernetAddress &target_ethernet_address) {
    // create Ethernet frame, set payload, type, src, dst
    // calc checksum
    EthernetFrame ip_frame;
    ip_frame.payload() = dgram.serialize();
    ip_frame.header().type = EthernetHeader::TYPE_IPv4;
    ip_frame.header().src = _ethernet_address;
    ip_frame.header().dst = target_ethernet_address;
    _frames_out.push(ip_frame);
}

void NetworkInterface::update_ip_ethernet_cache(const uint32_t ip_address, const EthernetAddress &ethernet_address) {
    _ip_ethernet_cache[ip_address] = ethernet_address;
    _ip_expire_time[ip_address] = _ms_since_running + 30000;
    _arp_expire_time.erase(ip_address);
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    const auto it = _ip_ethernet_cache.find(next_hop_ip);
    const auto it_time = _ip_expire_time.find(next_hop_ip);
    if (it == _ip_ethernet_cache.end() || it_time == _ip_expire_time.end() || it_time->second < _ms_since_running) {
        const auto it_arp = _arp_expire_time.find(next_hop_ip);
        // check if we have sent this five second ago
        if (it_arp != _ip_expire_time.end() && it_arp->second >= _ms_since_running) {
            return;
        }
        send_arp_message(next_hop_ip, ETHERNET_BROADCAST, ARPMessage::OPCODE_REQUEST);
        _blocked_ip_datagram.emplace(next_hop_ip, dgram);
        _arp_expire_time[next_hop_ip] = _ms_since_running + 5000;
        return;
    }

    send_ip_dgram(dgram, it->second);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // ignore any frames not destined for the network interface
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;
        if (arp_message.parse(frame.payload()) != ParseResult::NoError) {
            return nullopt;
        }
        if (arp_message.opcode == ARPMessage::OPCODE_REPLY) {
            update_ip_ethernet_cache(arp_message.sender_ip_address, arp_message.sender_ethernet_address);
            while (!_blocked_ip_datagram.empty()) {
                const auto [next_hop_ip, dgram] = _blocked_ip_datagram.front();
                const auto it = _ip_ethernet_cache.find(next_hop_ip);
                const auto it_time = _ip_expire_time.find(next_hop_ip);
                if (it == _ip_ethernet_cache.end() || it_time == _ip_expire_time.end() ||
                    it_time->second < _ms_since_running) {
                    break;
                }
                send_ip_dgram(dgram, it->second);
                _blocked_ip_datagram.pop();
            }
            return nullopt;
        } else if (arp_message.opcode != ARPMessage::OPCODE_REQUEST) {
            return nullopt;
        }
        // if arp_message.opcode == ARPMessage::OPCODE_REQUEST
        update_ip_ethernet_cache(arp_message.sender_ip_address, arp_message.sender_ethernet_address);
        if (arp_message.target_ip_address == _ip_address.ipv4_numeric()) {
            send_arp_message(
                arp_message.sender_ip_address, arp_message.sender_ethernet_address, ARPMessage::OPCODE_REPLY);
        }
        return nullopt;
    } else if (frame.header().type != EthernetHeader::TYPE_IPv4) {
        return nullopt;
    }

    // for IPv4 frame, parse the payload as an InternetDatagram
    InternetDatagram dgram;
    if (dgram.parse(frame.payload()) != ParseResult::NoError) {
        return nullopt;
    }

    return dgram;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _ms_since_running += ms_since_last_tick;

    for (auto it = _ip_expire_time.begin(); it != _ip_expire_time.end();) {
        if (it->second < _ms_since_running) {
            _ip_ethernet_cache.erase(it->first);
            it = _ip_expire_time.erase(it);
        } else {
            ++it;
        }
    }
}
