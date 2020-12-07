#pragma once

#include <string_view>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {

/**
 * Provides functions to extract bytes from a bytes buffer.
 */
// TODO(yzhao): Merge with code in src/stirling/protocols/cql/frame_body_decoder.{h,cc}.
class BinaryDecoder {
 public:
  explicit BinaryDecoder(std::string_view buf) : buf_(buf) {}

  bool eof() const { return buf_.empty(); }
  size_t BufSize() const { return buf_.size(); }
  std::string_view Buf() const { return buf_; }

  template <typename TCharType = char>
  StatusOr<TCharType> ExtractChar() {
    static_assert(sizeof(TCharType) == 1);
    if (buf_.size() < sizeof(TCharType)) {
      return error::ResourceUnavailable("Insufficient number of bytes.");
    }
    TCharType res = buf_.front();
    buf_.remove_prefix(1);
    return res;
  }

  template <typename TIntType>
  StatusOr<TIntType> ExtractInt() {
    if (buf_.size() < sizeof(TIntType)) {
      return error::ResourceUnavailable("Insufficient number of bytes.");
    }
    TIntType val = ::pl::utils::BEndianBytesToInt<TIntType>(buf_);
    buf_.remove_prefix(sizeof(TIntType));
    return val;
  }

  template <typename TCharType = char>
  StatusOr<std::basic_string_view<TCharType>> ExtractString(size_t len) {
    static_assert(sizeof(TCharType) == 1);
    if (buf_.size() < len) {
      return error::ResourceUnavailable("Insufficient number of bytes.");
    }
    auto tbuf = CreateStringView<TCharType>(buf_);
    buf_.remove_prefix(len);
    return tbuf.substr(0, len);
  }

  // Extract until encounter the input sentinel character.
  // The sentinel character is not returned, but is still removed from the buffer.
  template <typename TCharType = char>
  StatusOr<std::basic_string_view<TCharType>> ExtractStringUntil(TCharType sentinel) {
    static_assert(sizeof(TCharType) == 1);
    auto tbuf = CreateStringView<TCharType>(buf_);
    size_t pos = tbuf.find(sentinel);
    if (pos == std::string_view::npos) {
      return error::NotFound("Could not find sentinel character");
    }
    buf_.remove_prefix(pos + 1);
    return tbuf.substr(0, pos);
  }

  // An overloaded version to look for sentinel string instead of a char.
  template <typename TCharType = char>
  StatusOr<std::basic_string_view<TCharType>> ExtractStringUntil(
      std::basic_string_view<TCharType> sentinel) {
    static_assert(sizeof(TCharType) == 1);
    auto tbuf = CreateStringView<TCharType>(buf_);
    size_t pos = tbuf.find(sentinel);
    if (pos == std::string_view::npos) {
      return error::NotFound("Could not find sentinel character");
    }
    buf_.remove_prefix(pos + sentinel.size());
    return tbuf.substr(0, pos);
  }

  template <typename TCharType = char>
  StatusOr<std::basic_string_view<TCharType>> ExtractStringUntil(const TCharType* sentinel) {
    return ExtractStringUntil<TCharType>(std::basic_string_view<TCharType>(sentinel));
  }

 private:
  std::string_view buf_;
};

}  // namespace stirling
}  // namespace pl
