#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace std;

void Reassembler::flush()
{
  size_t length = 0;
  while ( length < buffer_.size() && buffer_[length].has_value() ) {
    ++length;
  }
  if ( length == 0 )
    return;

  string out {};
  out.reserve( length );
  for ( size_t i = 0; i < length; ++i ) {
    out.push_back( *buffer_[i] );
  }
  output_.writer().push( out );

  next_index_ += length;
  unassembler_bytes_ -= length;

  vector<optional<char>> newbuf( buffer_.size() );
  for ( size_t i = length; i < buffer_.size(); ++i ) {
    newbuf[i - length] = buffer_[i];
  }
  buffer_.swap( newbuf );
}

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  assert( !buffer_.empty() );
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  if ( is_last_substring ) {
    eof_ = true;
    eof_index_ = first_index + data.size();
  }

  auto start = std::max( next_index_, first_index );
  auto end = std::min( first_index + data.size(), next_index_ + output_.writer().available_capacity() );

  if ( start >= end ) {
    flush();
    if ( eof_ && next_index_ == eof_index_ )
      output_.writer().close();
    return;
  }

  for ( auto i = start; i < end; ++i ) {
    size_t offset = static_cast<size_t>( i - next_index_ );
    size_t di = static_cast<size_t>( i - first_index );
    if ( !buffer_[offset].has_value() ) {
      buffer_[offset] = data[di];
      unassembler_bytes_++;
    }
  }
  flush();
  if ( eof_ && next_index_ == eof_index_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  // debug( "unimplemented count_bytes_pending() called" );
  return unassembler_bytes_;
}
