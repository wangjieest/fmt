/*
 Formatting library for C++ - string utilities

 Copyright (c) 2012 - 2016, Victor Zverovich
 All rights reserved.

 For the license information refer to format.h.
 */

#ifdef FMT_INCLUDE
# error "Add the fmt's parent directory and not fmt itself to includes."
#endif

#ifndef FMT_STRING_H_
#define FMT_STRING_H_

#include "format.h"

namespace fmt {

namespace internal {

// A buffer that stores data in ``std::basic_string``.
template <typename Char, typename Allocator = std::allocator<Char> >
class StringBuffer : public Buffer<Char> {
 public:
  typedef std::basic_string<Char, std::char_traits<Char>, Allocator> StringType;

 private:
  StringType data_;

 protected:
  virtual void grow(std::size_t size) FMT_OVERRIDE {
    data_.resize(size);
    this->ptr_ = &data_[0];
    this->capacity_ = size;
  }

 public:
  explicit StringBuffer(const Allocator &allocator = Allocator())
  : data_(allocator) {}

  // Moves the data to ``str`` clearing the buffer.
  void move_to(StringType &str) {
    data_.resize(this->size_);
    str.swap(data_);
    this->capacity_ = this->size_ = 0;
    this->ptr_ = FMT_NULL;
  }
  StringType move_str() {
    data_.resize(this->size_);
    this->capacity_ = this->size_ = 0;
    this->ptr_ = FMT_NULL;
    return std::move(data_);
  }
  void swap(StringType &str) FMT_NOEXCEPT {
    data_.resize(this->size_);
    str.swap(data_);
    this->capacity_ = this->size_ = data_.size();
    this->ptr_ = &data_[0];
  }
  const StringType& string_ref() {
      data_.resize(this->size_);
      return data_;
  }
};
}  // namespace internal

/**
  \rst
  This class template provides operations for formatting and writing data
  into a character stream. The output is stored in a ``std::basic_string``
  that grows dynamically.

  You can use one of the following typedefs for common character types
  and the standard allocator:

  +---------------+----------------------------+
  | Type          | Definition                 |
  +===============+============================+
  | StringWriter  | BasicStringWriter<char>    |
  +---------------+----------------------------+
  | WStringWriter | BasicStringWriter<wchar_t> |
  +---------------+----------------------------+

  **Example**::

     StringWriter out;
     out << "The answer is " << 42 << "\n";

  This will write the following output to the ``out`` object:

  .. code-block:: none

     The answer is 42

  The output can be moved to a ``std::basic_string`` with ``out.move_to()``.
  \endrst
 */
template <typename Char, typename Allocator = std::allocator<Char> >
class BasicStringWriter : public BasicWriter<Char> {
 private:
  internal::StringBuffer<Char, Allocator> buffer_;
  typedef std::basic_string<Char, std::char_traits<Char>, Allocator> StringType;

 public:
  /**
    \rst
    Constructs a :class:`fmt::BasicStringWriter` object.
    \endrst
   */
  explicit BasicStringWriter(const Allocator &allocator = Allocator())
  : BasicWriter<Char>(buffer_), buffer_(allocator) {}

  /**
    \rst
    Moves the buffer content to *str* clearing the buffer.
    \endrst
   */
  void move_to(StringType& str) {
    buffer_.move_to(str);
  }

  void swap(StringType& str) FMT_NOEXCEPT {
      buffer_.swap(str);
  }

  StringType move_str() {
      return buffer_.move_str();
  }

  const StringType& string_ref() {
      return buffer_.string_ref();
  }
};

typedef BasicStringWriter<char> StringWriter;
typedef BasicStringWriter<wchar_t> WStringWriter;

/**
  \rst
  Converts *value* to ``std::string`` using the default format for type *T*.

  **Example**::

    #include "fmt/string.h"

    std::string answer = fmt::to_string(42);
  \endrst
 */
template <typename T>
std::string to_string(const T &value) {
  fmt::StringWriter w;
  w << value;
  return w.move_str();
}

// move support format
inline std::string format_str(CStringRef format_string, ArgList args) {
  fmt::StringWriter w;
  w.write(format_string, args);
  return w.move_str();
}

inline std::wstring format_str(WCStringRef format_string, ArgList args) {
  fmt::WStringWriter w;
  w.write(format_string, args);
  return w.move_str();
}
FMT_VARIADIC(std::string, format_str, CStringRef)
FMT_VARIADIC_W(std::wstring, format_str, WCStringRef)

// support hex view output
struct hex_view {
  hex_view(const void* data, size_t len) : data_(data), len_(len) {}
  hex_view(const std::string& str) : data_(str.data()), len_(str.size()) {}
  hex_view(const std::vector<char>& vec) : data_(vec.data()), len_(vec.size()) {}
  const void* data_;
  size_t len_;
};

template<typename Char>
void format_arg(fmt::BasicFormatter<Char>& f,
      const Char*& fmt_str,
      const hex_view& s) {
  const Char *end = fmt_str;
  while (*end && *end != '}')
      ++end;
  if (*end != '}')
      FMT_THROW(FormatError("missing '}' in format string"));
  fmt_str = end + 1;
  if (!s.len_ || !s.data_)
      return;

  Buffer<Char> &buffer = f.writer().buffer();
  const std::size_t start = buffer.size();
  buffer.resize(s.len_ * 2 + start);
  const char* numbers = "0123456789ABCDEF";
  const char* data = reinterpret_cast<const char*>(s.data_);
  Char*ptr = &buffer[start];
  for (size_t i = 0; i < s.len_; ++i) {
    ptr[i * 2] = numbers[((data[i]) & 0xF0) >> 4];
    ptr[i * 2 + 1] = numbers[((data[i]) & 0xF)];
  }
}
}

#endif  // FMT_STRING_H_
