#include "src/stirling/pgsql/parse.h"

#include <arpa/inet.h>

#include <string>
#include <utility>

#include <absl/strings/ascii.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace pgsql {

struct BinParser {
  std::string_view data;

  bool Empty() const { return data.empty(); }

  size_t DataSize() const { return data.size(); }

  char ReadChar() {
    char res = data.front();
    data.remove_prefix(1);
    return res;
  }
  int32_t ReadBEInt32() {
    const auto res = utils::BEndianBytesToInt<int32_t>(data);
    data.remove_prefix(sizeof(int32_t));
    return res;
  }
  std::string ReadString(size_t len) {
    auto res = std::string(data.substr(0, len));
    data.remove_prefix(len);
    return res;
  }
};

ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg) {
  constexpr int kMinMsgLen = 1 + sizeof(int32_t);
  if (buf->size() < kMinMsgLen) {
    return ParseState::kNeedsMoreData;
  }

  BinParser parser{*buf};
  msg->tag = parser.ReadChar();
  msg->len = parser.ReadBEInt32();
  constexpr int kLenFieldLen = 4;
  if (msg->len < kLenFieldLen) {
    // Len includes the len field itself, so its value cannot be less than the length of the field.
    return ParseState::kInvalid;
  }
  const size_t str_len = msg->len - 4;
  if (parser.DataSize() < str_len) {
    return ParseState::kNeedsMoreData;
  }
  // Len includes the length field itself (int32_t), so the payload needs to exclude 4 bytes.
  msg->payload = parser.ReadString(str_len);
  return ParseState::kSuccess;
}

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
