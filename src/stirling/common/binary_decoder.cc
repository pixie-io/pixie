#include "src/stirling/common/binary_decoder.h"

#include "src/common/base/base.h"

namespace pl {
namespace stirling {

char BinaryDecoder::ExtractChar() {
  char res = buf_.front();
  buf_.remove_prefix(1);
  return res;
}

// Extract until encounter the input sentinel character.
// The sentinel character is returned, and removed from the buffer.
std::string_view BinaryDecoder::ExtractStringUtil(char sentinel) {
  size_t pos = buf_.find(sentinel);
  if (pos == std::string_view::npos) {
    return {};
  }
  auto res = buf_.substr(0, pos);
  buf_.remove_prefix(pos + 1);
  return res;
}

}  // namespace stirling
}  // namespace pl
