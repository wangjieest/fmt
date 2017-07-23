/*
 Formatting library for C++ - std::ostream support

 Copyright (c) 2012 - 2016, Victor Zverovich
 All rights reserved.

 For the license information refer to format.h.
 */

#include "ostream.h"

namespace fmt {

namespace internal {
FMT_FUNC void write(std::ostream &os, Writer &w) {
  const char *data = w.data();
  typedef internal::MakeUnsigned<std::streamsize>::Type UnsignedStreamSize;
  UnsignedStreamSize size = w.size();
  UnsignedStreamSize max_size =
      internal::to_unsigned((std::numeric_limits<std::streamsize>::max)());
  do {
    UnsignedStreamSize n = size <= max_size ? size : max_size;
    os.write(data, static_cast<std::streamsize>(n));
    data += n;
    size -= n;
  } while (size != 0);
}
}

FMT_FUNC void print(std::ostream &os, CStringRef format_str, ArgList args) {
  MemoryWriter w;
  w.write(format_str, args);
  internal::write(os, w);
}

namespace internal {
template <typename Char, size_t N>
class BasicStreamBuffer : public Buffer<Char> {
 private:
  Char data_[N];
  std::basic_ostream<Char> & stream_;
 protected:
  virtual void grow(std::size_t /*size*/) FMT_OVERRIDE {
    reset();
  }
  void reset() FMT_NOEXCEPT {
    if (this->size_)  // output to stream
      stream_.write(this->ptr_, static_cast<std::streamsize>(this->size_));
    this->ptr_ = data_;
    this->size_ = 0;
  }
 public:
  explicit BasicStreamBuffer(std::basic_ostream<Char> & os) FMT_NOEXCEPT
    : stream_(os) {
    this->capacity_ = N;
    this->ptr_ = data_;
    this->size_ = 0;
  }
  ~BasicStreamBuffer() FMT_NOEXCEPT {
    reset();
  }
};
template <typename Char, size_t N = 500u>
class BasicStreamWriter : public BasicWriter<Char> {
 private:
  BasicStreamBuffer<Char, N> buffer_;
 public:
  explicit BasicStreamWriter(std::basic_ostream<Char> & os)
    : BasicWriter<Char>(buffer_), buffer_(os) {}
};
typedef BasicStreamWriter<char> StreamWriter;
typedef BasicStreamWriter<wchar_t> WStreamWriter;
}  // namespace internal

FMT_FUNC void format(std::ostream &os, CStringRef format_str, ArgList args) {
  internal::StreamWriter w(os);
  w.write(format_str, args);
}
FMT_FUNC void format(std::wostream &os, WCStringRef format_str, ArgList args) {
  internal::WStreamWriter w(os);
  w.write(format_str, args);
}
}  // namespace fmt
