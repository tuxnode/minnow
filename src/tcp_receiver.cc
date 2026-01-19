#include "tcp_receiver.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // debug( "unimplemented receive() called" );

  // 处理错误
  if ( message.RST ) {
    reassembler_.reader().set_error();
  }

  // 处理SYN标志
  if ( message.SYN ) {
    isn_ = message.seqno;
    synced_ = true;
  }

  if ( !synced_ )
    return;

  uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  uint64_t abs_seqno = message.seqno.unwrap( isn_.value(), checkpoint );

  uint64_t first_index = ( message.SYN ) ? 0 : abs_seqno - 1;
  reassembler_.insert( first_index, std::move( message.payload ), message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // debug( "unimplemented send() called" );

  TCPReceiverMessage msg {};
  uint64_t aval_cap = reassembler_.writer().available_capacity();
  msg.window_size = static_cast<uint16_t>( std::min( aval_cap, static_cast<uint64_t>( UINT16_MAX ) ) );

  if ( synced_ ) {
    uint64_t abs_ackno = reassembler_.writer().bytes_pushed() + 1;

    if ( reassembler_.writer().is_closed() )
      abs_ackno++;

    msg.ackno = Wrap32::wrap( abs_ackno, isn_.value() );
  }

  msg.RST = reassembler_.reader().has_error();

  return msg;
}
