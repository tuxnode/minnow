#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <string>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_seqno_ - acknowledged_seqno_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return resend_count_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t effict_window_size = ( window_size_ == 0 ) ? 1 : window_size_;

  while ( true ) {
    // 获取当前还需要确认
    uint64_t outstanding_size = sequence_numbers_in_flight();

    // 如果超出接收方缓冲接收能力
    if ( effict_window_size <= outstanding_size )
      break;

    TCPSenderMessage msg {};

    uint64_t free_space = effict_window_size - outstanding_size;
    // 处理SYN标志
    if ( !syn_send_ ) {
      msg.SYN = true;
      syn_send_ = true;
    }

    // 确定payload大小，三者取最小
    uint64_t remaining_space = free_space - msg.SYN;
    uint64_t payload_size = std::min(
      { remaining_space, reader().bytes_buffered(), static_cast<uint64_t>( TCPConfig::MAX_PAYLOAD_SIZE ) } );

    if ( payload_size > 0 ) {
      msg.payload = std::string( reader().peek().substr( 0, payload_size ) );
      input_.reader().pop( payload_size );
    }

    // 处理FIN
    // 条件：Stream 结束了、还没发过 FIN、窗口还有位置放 FIN
    if ( !fin_send_ && reader().is_finished() && ( msg.sequence_length() < free_space ) ) {
      msg.FIN = true;
      fin_send_ = true;
    }

    if ( msg.sequence_length() == 0 )
      break;

    // 输入seqno
    msg.seqno = Wrap32::wrap( next_seqno_, isn_ );

    if ( reader().has_error() )
      msg.RST = true;

    transmit( msg );

    if ( !time_runnning_ ) {
      time_runnning_ = true;
      timer_ms_ = 0;
    }

    // 添加到待确认缓冲区
    segment_.push_back( msg );
    next_seqno_ += msg.sequence_length();
    if ( msg.FIN )
      break;
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage emptr_msg { .seqno = Wrap32::wrap( next_seqno_, isn_ ),
                               .SYN = false,
                               .payload {},
                               .FIN = false,
                               .RST = reader().has_error() };
  return emptr_msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 判断RST标志位
  if ( msg.RST ) {
    reader().set_error();
    return;
  }

  window_size_ = msg.window_size;

  if ( !msg.ackno.has_value() )
    return;

  uint64_t abs_ackno = msg.ackno->unwrap( isn_, acknowledged_seqno_ );

  if ( abs_ackno > next_seqno_ )
    return;

  bool has_new_data_ack = false;
  // 循环清理已确认segment
  while ( !segment_.empty() ) {
    auto& first_msg = segment_.front();
    uint64_t abs_seqno_end = first_msg.seqno.unwrap( isn_, acknowledged_seqno_ ) + first_msg.sequence_length();

    if ( abs_seqno_end <= abs_ackno ) {
      segment_.pop_front();
      acknowledged_seqno_ = abs_seqno_end;
      has_new_data_ack = true;
    } else
      break;
  }

  if ( has_new_data_ack ) {
    current_RTO_ms_ = initial_RTO_ms_;
    resend_count_ = 0;
    timer_ms_ = 0;
    time_runnning_ = !segment_.empty();
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if ( !time_runnning_ )
    return;

  timer_ms_ += ms_since_last_tick;

  if ( timer_ms_ >= current_RTO_ms_ ) {
    if ( !segment_.empty() ) {
      // 重传队列中最老的一个包
      transmit( segment_.front() );

      // 指数退避逻辑：窗口不为0时才增加 RTO 和重传计数
      if ( window_size_ > 0 ) {
        resend_count_++;
        current_RTO_ms_ *= 2;
      }
    }
    timer_ms_ = 0; // 重置当前计时器的值
  }
}
