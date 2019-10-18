#include <fstream>

#include "src/stirling/message_types.h"

namespace pl {
namespace stirling {

TEST(GetMessageType, Basic) {
  bool http_check =
      std::is_same_v<GetMessageType<ReqRespPair<http::HTTPMessage>>::type, http::HTTPMessage>;
  EXPECT_TRUE(http_check);

  bool http2_check =
      std::is_same_v<GetMessageType<ReqRespPair<http2::GRPCMessage>>::type, http2::Frame>;
  EXPECT_TRUE(http2_check);

  bool mysql_check = std::is_same_v<GetMessageType<mysql::Entry>::type, mysql::Packet>;
  EXPECT_TRUE(mysql_check);
}

TEST(GetMessageType, Mismatch) {
  bool check = std::is_same_v<GetMessageType<ReqRespPair<http::HTTPMessage>>::type, mysql::Packet>;
  EXPECT_FALSE(check);
}

}  // namespace stirling
}  // namespace pl
