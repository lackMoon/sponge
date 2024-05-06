#include "network_interface.hh"

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
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

bool NetworkInterface::is_expired(uint32_t ip_address) {
    auto &item = _arp_table.at(ip_address);
    bool ret = item._entry_time <= _time && _time - item._entry_time > EXPIRED_TIME;
    _expired_hits -= ret;
    return ret;
}

void NetworkInterface::send_frame(uint16_t type, uint32_t ip_address) {
    EthernetFrame frame;
    auto &header = frame.header();
    auto &item = _arp_table.at(ip_address);
    header.type = type;
    header.src = _ethernet_address;
    header.dst = item._ethernet_address;
    if (type == EthernetHeader::TYPE_IPv4) {  // Ipv4 datagram
        auto &pending_queue = item._pending_dgrams;
        while (!pending_queue.empty()) {
            auto ip_frame = frame;
            ip_frame.payload() = std::move(pending_queue.front()).serialize();
            _frames_out.emplace(std::move(ip_frame));
            pending_queue.pop();
        }
    } else {
        ARPMessage msg;
        msg.sender_ethernet_address = _ethernet_address;
        msg.sender_ip_address = _ip_address.ipv4_numeric();
        msg.target_ip_address = ip_address;
        if (header.dst == ETHERNET_BROADCAST) {  // ARP Request
            msg.opcode = ARPMessage::OPCODE_REQUEST;
            if (item._last_request_time <= _time && _time - item._last_request_time <= CACHE_TIME) {
                return;
            }
            item._last_request_time = _time;
        } else {
            msg.opcode = ARPMessage::OPCODE_REPLY;
            msg.target_ethernet_address = header.dst;
        }
        frame.payload() = msg.serialize();
        _frames_out.emplace(std::move(frame));
    }
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    uint16_t type = EthernetHeader::TYPE_ARP;
    if (_arp_table.count(next_hop_ip) && !is_expired(next_hop_ip)) {
        if (_arp_table.at(next_hop_ip)._ethernet_address != ETHERNET_BROADCAST) {
            type = EthernetHeader::TYPE_IPv4;
        }
    } else {
        _arp_table[next_hop_ip] = {};
    }
    _arp_table.at(next_hop_ip)._pending_dgrams.emplace(std::move(dgram));
    send_frame(type, next_hop_ip);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    auto &header = frame.header();
    auto &dst = header.dst;
    if (!(dst == ETHERNET_BROADCAST || dst == _ethernet_address)) {
        return std::nullopt;
    }
    if (header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram dgram;
        if (ParseResult::NoError == dgram.parse(frame.payload())) {
            return dgram;
        }
    } else {
        ARPMessage arp_msg;
        if (ParseResult::NoError == arp_msg.parse(frame.payload())) {
            auto sender_ip = arp_msg.sender_ip_address;
            auto &sender_mac = arp_msg.sender_ethernet_address;
            auto dst_ip = arp_msg.target_ip_address;
            if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {
                auto &item = _arp_table.at(sender_ip);
                item._ethernet_address = sender_mac;
                item._entry_time = _time;
                send_frame(EthernetHeader::TYPE_IPv4, sender_ip);
            } else {
                if (!_arp_table.count(sender_ip)) {
                    _arp_table[sender_ip] = {sender_mac, {}, _time};
                }
                if (dst_ip == _ip_address.ipv4_numeric()) {
                    send_frame(EthernetHeader::TYPE_ARP, sender_ip);
                }
            }
        }
    }
    return std::nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
    if (_expired_hits <= 0) {
        for (auto &item : _arp_table) {
            if (is_expired(item.first)) {
                _arp_table.erase(item.first);
            }
        }
        _expired_hits = 5;
    }
}
