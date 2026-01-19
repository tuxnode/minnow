#include "wrapping_integers.hh"
#include <cmath>
#include <cstdint>
// #include "debug.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // debug( "unimplemented wrap( {}, {} ) called", n, zero_point.raw_value_ );
  return Wrap32 { zero_point.raw_value_ + static_cast<uint32_t>( n ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // debug( "unimplemented unwrap( {}, {} ) called", zero_point.raw_value_, checkpoint );
  uint32_t offset = raw_value_ - zero_point.raw_value_;
  // 选定一个候选值，还没有将其和checkpoint处理
  uint64_t candidate = ( checkpoint & 0xFFFFFFFF00000000 ) + static_cast<uint64_t>( offset );
  uint64_t ret = candidate;
  // 定义一个uint32_t循环周期
  uint64_t step = 1ULL << 32; // 1ULL代表字面量为1的unsigned long long

  // 找到离checkpoint更近的位置
  if ( candidate < checkpoint ) {
    if ( checkpoint - candidate > ( step / 2 ) ) {
      ret = candidate + step;
    }
  } else {
    if ( candidate - checkpoint > ( step / 2 ) && candidate >= step ) {
      ret = candidate - step;
    }
  }

  return ret;
}
