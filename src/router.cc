#include "router.hh"
#include "address.hh"
#include "debug.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

#include <cstdint>
#include <iostream>
#include <utility>

using namespace std;

// 子网掩码计算
// 路由匹配
// 最长前缀匹配机制，前缀越长，匹配越精确

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  // cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
  //      << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
  //      << " on interface " << interface_num << "\n";

  // debug( "unimplemented add_route() called" );
  RouteEntry route{};
  route.route_prefix = route_prefix;
  route.prefix_length = prefix_length;
  route.next_hop = next_hop;
  route.interface_num = interface_num;

  routing_table_.push_back(route);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // debug( "unimplemented route() called" );
  for (auto &net_interface : interfaces_) {
    auto &recv_que = net_interface->datagrams_received();

    while (not recv_que.empty()) {
      InternetDatagram dgram = std::move(recv_que.front());
      recv_que.pop();

      // 处理TTL
      if (dgram.header.ttl <= 1) continue;
      dgram.header.ttl--;

      // 重新计算校验和
      dgram.header.compute_checksum();

      const uint32_t dst_ip = dgram.header.dst;
      std::optional<RouteEntry> best_route{}; // 最长前缀匹配机制

      for (auto &iter : routing_table_) {
        // 计算掩码
        uint32_t mask = (iter.prefix_length == 0) ? 0 : 0xFFFFFFFF << (32 - iter.prefix_length);

        // 确定网段
        if ((dst_ip & mask) == (iter.route_prefix & mask)) {
          // 前缀越长，匹配越精确
          if (not best_route.has_value() || iter.prefix_length > best_route->prefix_length) {
            best_route = iter;
          }
        }
      }
      if (best_route.has_value()) {
        // 有值则取值，无值则默认值
        Address next_hop = best_route->next_hop.value_or(Address::from_ipv4_numeric(dst_ip));

        interfaces_[best_route->interface_num]->send_datagram(dgram, next_hop);
      }
    }
  }
}
