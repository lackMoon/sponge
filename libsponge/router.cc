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

bool Router::is_match(uint32_t lval, uint32_t rval, uint8_t match_prefix) {
    return !(match_prefix && ((lval ^ rval) >> (ADDRESS_SIZE - match_prefix)));
}

void Router::add_router_node(std::shared_ptr<RouterNode> &curr_node,
                             const uint32_t route_prefix,
                             const uint8_t prefix_length,
                             const std::optional<Address> &next_hop,
                             const size_t interface_num) {
    auto index_bits = RouterNode::INDEX_PREFIX_LENGTH;
    if (curr_node == nullptr) {
        curr_node = std::make_shared<RouterNode>();
    }
    if (prefix_length < index_bits) {
        curr_node->_value = new RouteInfo{route_prefix, prefix_length, next_hop, interface_num, curr_node->_value};
    } else {
        auto index = route_prefix >> (ADDRESS_SIZE - index_bits);
        add_router_node(curr_node->_children.at(index),
                        route_prefix << index_bits,
                        prefix_length - index_bits,
                        next_hop,
                        interface_num);
    }
}

bool Router::search_route_info(std::shared_ptr<RouterNode> &curr_node,
                               const uint32_t match_address,
                               InternetDatagram &dgram) {
    auto index_bits = RouterNode::INDEX_PREFIX_LENGTH;
    auto index = match_address >> (ADDRESS_SIZE - index_bits);
    auto child_node = curr_node->_children.at(index);
    if (child_node != nullptr && search_route_info(child_node, match_address << index_bits, dgram)) {
        return true;
    }
    auto route_info = curr_node->_value;
    RouteInfo *target_info = nullptr;
    uint8_t longest_prefix = 0;
    while (route_info != nullptr) {
        if (is_match(match_address, route_info->_key, route_info->_key_prefix) &&
            route_info->_key_prefix >= longest_prefix) {
            longest_prefix = route_info->_key_prefix;
            target_info = route_info;
        }
        route_info = route_info->_next;
    }
    if (target_info == nullptr) {
        return false;
    }
    auto next_hop = target_info->_next_hop.has_value() ? target_info->_next_hop.value()
                                                       : Address::from_ipv4_numeric(dgram.header().dst);
    interface(target_info->_interface_num).send_datagram(dgram, next_hop);
    return true;
}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    auto info_prefix = route_prefix & (UINT32_MAX << (ADDRESS_SIZE - prefix_length));
    add_router_node(_root, info_prefix, prefix_length, next_hop, interface_num);
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    auto &header = dgram.header();
    auto dst = header.dst;
    if (header.ttl-- <= 1) {
        return;
    }
    search_route_info(_root, dst, dgram);
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
