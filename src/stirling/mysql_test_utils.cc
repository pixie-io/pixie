#include "src/stirling/mysql_test_utils.h"
#include <string>
#include "src/stirling/utils/byte_format.h"

namespace pl {
namespace stirling {
namespace testutils {
std::string GenRequest(const ConstStrView& command, const std::string& msg) {
  return GenPacket(0, absl::StrCat(command, msg));
}

std::string GenErr(const std::string& msg) {
  std::string packet = absl::StrCat("\xff\x48\x04\x23\x48\x59\x30\x30\x30", msg);
  return GenPacket(1, packet);
}

std::string GenOk(const std::string& msg) {
  std::string packet = absl::StrCat(std::string("\x00\x00\x00\x02\x00\x00\x00", 7), msg);
  return GenPacket(1, packet);
}

std::string GenPacket(int packet_num, const std::string& msg) {
  char len_bytes[3];
  utils::IntToLEBytes<3>(msg.size(), len_bytes);
  return absl::StrCat(std::string(len_bytes, 3), packet_num, msg);
}
}  // namespace testutils
}  // namespace stirling
}  // namespace pl
