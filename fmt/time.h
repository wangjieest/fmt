/*
 Formatting library for C++ - time formatting

 Copyright (c) 2012 - 2016, Victor Zverovich
 All rights reserved.

 For the license information refer to format.h.
 */

#ifndef FMT_TIME_H_
#define FMT_TIME_H_

#include "format.h"
#include <ctime>
#define FMT_SUPPORT_SYSTEM_TIME
#ifdef FMT_SUPPORT_SYSTEM_TIME
#include <chrono>
#endif
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4702)  // unreachable code
# pragma warning(disable: 4996)  // "deprecated" functions
#endif
namespace fmt {

#ifdef FMT_SUPPORT_SYSTEM_TIME
  inline struct timespec systemtime_now() {
  struct timespec ts;
  using namespace std::chrono;
  // thanks @AndreasSchoenle for the implementation and the explanation:
  // The time since epoch for the steady_clock is not necessarily really the time since 1970.
  // It usually is the time since program start. Thus, here is calculated the offset between 
  // the starting point and the real start of the epoch as reported by the system clock 
  // with the precision of the system clock. 
  // 
  // Time stamps will later have system clock accuracy but relative times will have the precision
  // of the high resolution clock.   
  static const nanoseconds os_system = 
    time_point_cast<nanoseconds>(system_clock::now()).time_since_epoch();
  static const nanoseconds os_stable =
    time_point_cast<nanoseconds>(steady_clock::now()).time_since_epoch();
  static nanoseconds os = os_system - os_stable;

  // 32-bit system work-around, where apparenetly the os correction above could sometimes 
  // become negative. This correction will only be done once per thread
  if (os.count() < 0) {
    os = os_system;
  }
  long long now_ns = (time_point_cast<nanoseconds>(steady_clock::now()).
                       time_since_epoch() + os).count();
  const int kNanos = 1000000000;
  ts.tv_sec = now_ns / kNanos;
  ts.tv_nsec = now_ns % kNanos;
  //issue
  if (ts.tv_sec < 0)
    ts.tv_sec = 0;
  if (ts.tv_nsec < 0)
    ts.tv_nsec = 0;
  return ts;
}
namespace internal {
template<typename Char>
struct time_format_helper;
template<>
struct time_format_helper<char> {
    typedef char Char;
    static constexpr Char* zerostr = "000000000";
    static constexpr Char* fmtstr = "%Y-%m-%d_%H-%M-%S.%f";
    static constexpr Char* spec = "%f";
    static constexpr Char* holder = "{}";
    static size_t tcsftime(Char* strDest,
                    size_t maxsize,
                    const Char* format,
                    const struct tm* timeptr) {
        return std::strftime(strDest, maxsize, format, timeptr);
    }
};
template<>
struct time_format_helper<wchar_t> {
    typedef wchar_t Char;
    static constexpr Char* zerostr = L"000000000";
    static constexpr Char* fmtstr = L"%Y-%m-%d_%H-%M-%S.%f";
    static constexpr Char* spec = L"%f";
    static constexpr Char* holder = L"{}";
    static size_t tcsftime(Char* strDest,
                    size_t maxsize,
                    const Char* format,
                    const struct tm* timeptr) {
        return std::wcsftime(strDest, maxsize, format, timeptr);
    }
};
}
template <typename Char, typename ArgFormatter>
void format_arg(BasicFormatter<Char, ArgFormatter> &f,
  const Char *&format_str, const struct timespec &ts) {
  if (*format_str == ':')
    ++format_str;
  const Char *end = format_str;
  while (*end && *end != '}')
    ++end;
  if (*end != '}')
    FMT_THROW(FormatError("missing '}' in format string"));

  // replace all %f[1|2|3|4|5|6|7|8|9]
  typedef std::basic_string<Char, std::char_traits<Char>, std::allocator<Char>> StringType;
  StringType preformat(format_str, end);
  if (format_str == end) {
    preformat = internal::time_format_helper<Char>::fmtstr;
  }
  for (size_t pos = 0; 
      (pos = preformat.find(internal::time_format_helper<Char>::spec, pos)) != std::string::npos;
      pos += 2) {
    if (pos != 0) {
      size_t begin_pos = preformat.find_last_not_of('%', pos) + 1;
      if ((pos - begin_pos) & 0x1) continue;
    }

    StringType value = internal::time_format_helper<Char>::zerostr;
    long nsec = ts.tv_nsec;
    Char ch = '6';
    bool more = (preformat.size() > pos + 2);
    if(more){
      ch = preformat.at(pos + 2);
      if (ch <= '0' || ch > '9') {
          more = false;
          ch = '6';
      }
    }

    uint32_t padding = '9' - ch;
    switch (padding) {
    case 9:nsec /= 10;
    case 8:nsec /= 10;
    case 7:nsec /= 10;
    case 6:nsec /= 10;
    case 5:nsec /= 10;
    case 4:nsec /= 10;
    case 3:nsec /= 10;
    case 2:nsec /= 10;
    case 1:nsec /= 10;
    default:break;
    }

    value += fmt::format(internal::time_format_helper<Char>::holder, nsec);
    preformat.replace(pos, 2 + (more ? 1 : 0), value.substr(value.size() - ch + '0'));
  }

  internal::MemoryBuffer<Char, internal::INLINE_BUFFER_SIZE> format;
  format.append(preformat.c_str(), preformat.c_str() + preformat.size() + 1);
  Buffer<Char> &buffer = f.writer().buffer();
  std::size_t start = buffer.size();
  const std::size_t MIN_GROWTH = 30;
  if (buffer.capacity() == start) {
      buffer.reserve(buffer.capacity() + MIN_GROWTH);
  }
  std::tm time;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__) && !defined(__GNUC__))
  localtime_s(&time, &ts.tv_sec); // windsows
#else
  localtime_r(&ts.tv_sec, &time); // POSIX
#endif
  for (;;) {
    std::size_t size = buffer.capacity() - start;
    std::size_t count = 
      internal::time_format_helper<Char>::tcsftime(&buffer[start], size, &format[0], &time);
    if (count != 0) {
      buffer.resize(start + count);
      break;
    }
    if (size >= format.size() * 256) {
      // If the buffer is 256 times larger than the format string, assume
      // that `strftime` gives an empty result. There doesn't seem to be a
      // better way to distinguish the two cases:
      // https://github.com/fmtlib/fmt/issues/367
      break;
    }
    buffer.reserve(buffer.capacity() + (size > MIN_GROWTH ? size : MIN_GROWTH));
  }
  format_str = end + 1;
}
#endif  // FMT_SUPPORT_SYSTEM_TIME

template <typename ArgFormatter>
void format_arg(BasicFormatter<char, ArgFormatter> &f,
                const char *&format_str, const std::tm &tm) {
  if (*format_str == ':')
    ++format_str;
  const char *end = format_str;
  while (*end && *end != '}')
    ++end;
  if (*end != '}')
    FMT_THROW(FormatError("missing '}' in format string"));
  internal::MemoryBuffer<char, internal::INLINE_BUFFER_SIZE> format;
  format.append(format_str, end + 1);
  format[format.size() - 1] = '\0';
  Buffer<char> &buffer = f.writer().buffer();
  std::size_t start = buffer.size();
  for (;;) {
    std::size_t size = buffer.capacity() - start;
    std::size_t count = std::strftime(&buffer[start], size, &format[0], &tm);
    if (count != 0) {
      buffer.resize(start + count);
      break;
    }
    if (size >= format.size() * 256) {
      // If the buffer is 256 times larger than the format string, assume
      // that `strftime` gives an empty result. There doesn't seem to be a
      // better way to distinguish the two cases:
      // https://github.com/fmtlib/fmt/issues/367
      break;
    }
    const std::size_t MIN_GROWTH = 10;
    buffer.reserve(buffer.capacity() + (size > MIN_GROWTH ? size : MIN_GROWTH));
  }
  format_str = end + 1;
}

namespace internal{
inline Null<> localtime_r(...) { return Null<>(); }
inline Null<> localtime_s(...) { return Null<>(); }
inline Null<> gmtime_r(...) { return Null<>(); }
inline Null<> gmtime_s(...) { return Null<>(); }
}

// Thread-safe replacement for std::localtime
inline std::tm localtime(std::time_t time) {
  struct LocalTime {
    std::time_t time_;
    std::tm tm_;

    LocalTime(std::time_t t): time_(t) {}

    bool run() {
      using namespace fmt::internal;
      return handle(localtime_r(&time_, &tm_));
    }

    bool handle(std::tm *tm) { return tm != FMT_NULL; }

    bool handle(internal::Null<>) {
      using namespace fmt::internal;
      return fallback(localtime_s(&tm_, &time_));
    }

    bool fallback(int res) { return res == 0; }

    bool fallback(internal::Null<>) {
      using namespace fmt::internal;
      std::tm *tm = std::localtime(&time_);
      if (tm) tm_ = *tm;
      return tm != FMT_NULL;
    }
  };
  LocalTime lt(time);
  if (lt.run())
    return lt.tm_;
  // Too big time values may be unsupported.
  FMT_THROW(fmt::FormatError("time_t value out of range"));
  return std::tm();
}

// Thread-safe replacement for std::gmtime
inline std::tm gmtime(std::time_t time) {
  struct GMTime {
    std::time_t time_;
    std::tm tm_;

    GMTime(std::time_t t): time_(t) {}

    bool run() {
      using namespace fmt::internal;
      return handle(gmtime_r(&time_, &tm_));
    }

    bool handle(std::tm *tm) { return tm != FMT_NULL; }

    bool handle(internal::Null<>) {
      using namespace fmt::internal;
      return fallback(gmtime_s(&tm_, &time_));
    }

    bool fallback(int res) { return res == 0; }

    bool fallback(internal::Null<>) {
      std::tm *tm = std::gmtime(&time_);
      if (tm != FMT_NULL) tm_ = *tm;
      return tm != FMT_NULL;
    }
  };
  GMTime gt(time);
  if (gt.run())
    return gt.tm_;
  // Too big time values may be unsupported.
  FMT_THROW(fmt::FormatError("time_t value out of range"));
  return std::tm();
}
} //namespace fmt

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#endif  // FMT_TIME_H_
