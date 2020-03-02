#include <fstream>

#include "src/common/testing/testing.h"
#include "src/stirling/protocol_traits.h"

namespace pl {
namespace stirling {

TEST(ProtocolTraits, GetFrameType) {
  bool http_check = std::is_same_v<ProtocolTraits<http::Record>::frame_type, http::Message>;
  EXPECT_TRUE(http_check);

  bool http2_check = std::is_same_v<ProtocolTraits<http2::Record>::frame_type, http2::Frame>;
  EXPECT_TRUE(http2_check);

  bool mysql_check = std::is_same_v<ProtocolTraits<mysql::Record>::frame_type, mysql::Packet>;
  EXPECT_TRUE(mysql_check);
}

TEST(GetFrameType, GetFrameTypeMismatch) {
  bool check = std::is_same_v<ProtocolTraits<http::Record>::frame_type, mysql::Packet>;
  EXPECT_FALSE(check);
}

}  // namespace stirling
}  // namespace pl
