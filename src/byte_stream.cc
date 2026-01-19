#include "byte_stream.hh"
#include <algorithm>
#include <string_view>

using std::string;
using std::string_view;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // 计算写入字节数
  size_t writing_size = std::min( data.size(), available_capacity() );
  buffer_ += data.substr( 0, writing_size );
  total_push_ += writing_size;
}

void Writer::close()
{
  // Your code here.
  closed = true;
}

bool Writer::is_closed() const
{
  return closed;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer_.size();
}

uint64_t Writer::bytes_pushed() const
{
  return total_push_;
}

string_view Reader::peek() const
{
  return string_view( buffer_.data(), buffer_.size() );
}

void Reader::pop( uint64_t len )
{
  buffer_ = buffer_.substr( len );
  total_pop_ += len;
}

bool Reader::is_finished() const
{
  return closed && buffer_.empty();
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return total_pop_;
}
