#pragma once

#include <string_view>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {

/**
 * Provides functions to extract bytes from a bytes buffer.
 */
// TODO(yzhao): Merge with code in src/stirling/cql/frame_body_decoder.{h,cc}.
class BinaryDecoder {
 public:
  explicit BinaryDecoder(std::string_view buf) : buf_(buf) {}

  bool Empty() const { return buf_.empty(); }
  size_t BufSize() const { return buf_.size(); }
  std::string_view Buf() const { return buf_; }

  char ExtractChar();

  template <typename TIntType>
  TIntType ExtractInteger() {
    TIntType val = utils::BEndianBytesToInt<TIntType>(buf_);
    buf_.remove_prefix(sizeof(TIntType));
    return val;
  }

  std::string_view ExtractString(size_t len);

  // Extract until encounter the input sentinel character.
  // The sentinel character is not returned, but is still removed from the buffer.
  std::string_view ExtractStringUtil(char sentinel);

 private:
  std::string_view buf_;
};

}  // namespace stirling
}  // namespace pl
