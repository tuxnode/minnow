#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"
#include "ipv4_datagram.hh"
#include "parser.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", std::move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const auto next_hop_ip = next_hop.ipv4_numeric();

  auto it = arp_catch_.find( next_hop_ip );
  // 如果找到缓存，就会广播地址
  if ( it != arp_catch_.end() ) {
    Serializer ser {};
    EthernetFrame frame {};
    frame.header.src = ethernet_address_;
    frame.header.dst = it->second.mac;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    dgram.serialize( ser );
    frame.payload = ser.finish();
    transmit( frame );
    return;
  }

  // 如果没有在缓存中找到

  // 挂起
  pending_by_ip_[next_hop_ip].push_back( Pending { dgram, 0 } );

  // 限制同一个ip 最快5秒发一次
  const auto cd_it = arp_request_cooldown_ms_.find( next_hop_ip );
  if ( cd_it != arp_request_cooldown_ms_.end() && cd_it->second > 0 )
    return;

  // 发送广播消息
  ARPMessage arp {};
  arp.opcode = ARPMessage::OPCODE_REQUEST;
  arp.sender_ethernet_address = ethernet_address_;
  arp.sender_ip_address = ip_address_.ipv4_numeric();
  arp.target_ethernet_address = EthernetAddress { 0, 0, 0, 0, 0, 0 };
  arp.target_ip_address = next_hop_ip;

  Serializer seq_ser {};
  arp.serialize( seq_ser );

  EthernetFrame req {};
  req.header.src = ethernet_address_;
  req.header.dst = ETHERNET_BROADCAST;
  req.header.type = EthernetHeader::TYPE_ARP;
  req.payload = seq_ser.finish();

  transmit( req );
  arp_request_cooldown_ms_[next_hop_ip] = 5000;
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame ) 
{
  // 过滤
  if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) return;

  // 解析ipv4地址
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram data{};

    // 反序列化
    Parser par{frame.payload};
    data.parse(par);
    if (!par.has_error()) datagrams_received_.push(data);

    return;
  }

  // 解析arp协议
  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage msg{};
    // 反序列化
    Parser p{frame.payload};
    msg.parse(p);

    // 如果解析错误
    if (p.has_error() || !msg.supported()) return;

    // 处理缓存
    constexpr size_t ARP_TTL_MS = 30'000;
    arp_catch_[ msg.sender_ip_address ] = ArpEntry{ msg.sender_ethernet_address, ARP_TTL_MS };

    // 检查是否有未发送的缓存
    auto pend_it = pending_by_ip_.find(msg.sender_ip_address);
    if (pend_it != pending_by_ip_.end()) {
      for (auto &pending : pend_it->second) {
        EthernetFrame out{};
        out.header.src = ethernet_address_;
        out.header.dst = msg.sender_ethernet_address;
        out.header.type = EthernetHeader::TYPE_IPv4;

        Serializer s{};
        pending.datagrame.serialize(s);
        out.payload = s.finish();

        transmit(out);
      }
      pending_by_ip_.erase(pend_it);
    }

    if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric()) {
      ARPMessage reply{};
      reply.opcode = ARPMessage::OPCODE_REPLY;
      reply.sender_ethernet_address = ethernet_address_;
      reply.sender_ip_address = ip_address_.ipv4_numeric();
      reply.target_ethernet_address = msg.sender_ethernet_address;
      reply.target_ip_address = msg.sender_ip_address;

      EthernetFrame reply_frame{};
      reply_frame.header.src = ethernet_address_;
      reply_frame.header.dst = msg.sender_ethernet_address;
      reply_frame.header.type = EthernetHeader::TYPE_ARP;

      Serializer s{};
      reply.serialize(s);
      reply_frame.payload = s.finish();

      transmit(reply_frame);
    }
    return;
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // debug( "unimplemented tick({}) called", ms_since_last_tick );
  // 1) ARP cache：TTL 递减，过期删除（30s）
  for ( auto it = arp_catch_.begin(); it != arp_catch_.end(); ) {
    if ( it->second.ttl_ms <= ms_since_last_tick ) {
      it = arp_catch_.erase( it );
    } else {
      it->second.ttl_ms -= ms_since_last_tick;
      ++it;
    }
  }

  // 2) ARP request 冷却：递减，归零删除（5s）
  for ( auto it = arp_request_cooldown_ms_.begin(); it != arp_request_cooldown_ms_.end(); ) {
    if ( it->second <= ms_since_last_tick ) {
      it = arp_request_cooldown_ms_.erase( it );
    } else {
      it->second -= ms_since_last_tick;
      ++it;
    }
  }

  // 3) 挂起 datagram：等待过久则丢弃（建议 5s）
  constexpr size_t PENDING_TTL_MS = 5'000;

  for ( auto it = pending_by_ip_.begin(); it != pending_by_ip_.end(); ) {
    auto& vec = it->second;

    for ( auto vit = vec.begin(); vit != vec.end(); ) {
      vit->age_ms += ms_since_last_tick;

      if ( vit->age_ms > PENDING_TTL_MS ) {
        vit = vec.erase( vit );
      } else {
        ++vit;
      }
    }

    if ( vec.empty() ) {
      it = pending_by_ip_.erase( it );
    } else {
      ++it;
    }
  }
}
