#include "src/stirling/source_connectors/socket_tracer/protocols/redis/stitcher.h"

#include <string>

#include "src/common/testing/testing.h"

namespace pl {
namespace stirling {
namespace protocols {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::StrEq;

redis::Message CreatePubMsg(uint64_t ts_ns, std::string payload, std::string_view command) {
  redis::Message msg;
  msg.timestamp_ns = ts_ns;
  msg.payload = std::move(payload);
  msg.command = command;
  msg.is_published_message = true;
  return msg;
}

TEST(StitchFramesTest, PubMessagesRemoved) {
  std::deque<redis::Message> reqs;

  std::deque<redis::Message> resps;
  resps.push_back(CreatePubMsg(0, R"(["message", "foo", "test1"])", "MESSAGE"));
  resps.push_back(CreatePubMsg(1, R"(["message", "foo", "test2"])", "MESSAGE"));

  NoState no_state;

  RecordsWithErrorCount<redis::Record> res = StitchFrames<redis::Record>(&reqs, &resps, &no_state);
  EXPECT_EQ(res.error_count, 0);
  EXPECT_THAT(
      res.records,
      ElementsAre(Field(&redis::Record::resp,
                        Field(&redis::Message::payload, StrEq(R"(["message", "foo", "test1"])"))),
                  Field(&redis::Record::resp,
                        Field(&redis::Message::payload, StrEq(R"(["message", "foo", "test2"])")))));
  EXPECT_THAT(resps, IsEmpty());
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
