#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <deque>
#include <functional>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;                     // 记录初始化序列号
  uint64_t next_seqno_ {};         // 期望的下个序号
  uint64_t acknowledged_seqno_ {}; // 已经被对方确认的最大序列号
  uint64_t window_size_ { 1 };     // 临时存储窗口大小

  bool syn_send_ {};
  bool fin_send_ {};

  // 计时器需要
  uint64_t initial_RTO_ms_;
  bool time_runnning_ {}; // 是否开启计时器
  uint64_t timer_ms_ {};
  uint64_t resend_count_ {}; // 维护连续重传次数，决定等待时间
  uint64_t current_RTO_ms_ { initial_RTO_ms_ };

  std::deque<TCPSenderMessage> segment_ {}; // 等待确认队列
};
