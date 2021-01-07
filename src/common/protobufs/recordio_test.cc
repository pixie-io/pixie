#include "src/common/protobufs/recordio.h"

#include <fstream>
#include <sstream>

#include "src/common/testing/testing.h"
#include "src/stirling/socket_tracer/protocols/http2/testing/proto/greet.pb.h"

namespace pl {
namespace rio {

using ::pl::stirling::protocols::http2::testing::HelloRequest;
using ::pl::testing::TempDir;
using ::pl::testing::proto::EqualsProto;

TEST(RecordIOTest, WriteAndRead) {
  std::stringstream ss;
  HelloRequest req;

  req.set_name("foo");
  EXPECT_TRUE(SerializeToStream(req, &ss));

  req.set_name("bar");
  EXPECT_TRUE(SerializeToStream(req, &ss));

  Reader reader(&ss);
  HelloRequest req_copy;

  EXPECT_TRUE(reader.Read(&req_copy));
  EXPECT_FALSE(reader.eof());
  EXPECT_THAT(req_copy, EqualsProto("name: 'foo'"));

  EXPECT_TRUE(reader.Read(&req_copy));
  EXPECT_FALSE(reader.eof());
  EXPECT_THAT(req_copy, EqualsProto("name: 'bar'"));

  EXPECT_FALSE(reader.Read(&req_copy));
  EXPECT_TRUE(reader.eof());
}

TEST(RecordIOTest, ReadInvalidData) {
  std::stringstream ss(
      "\x0F"
      "abcd");
  Reader reader(&ss);
  HelloRequest req_copy;
  EXPECT_FALSE(reader.Read(&req_copy));
  EXPECT_FALSE(reader.eof());
  EXPECT_FALSE(reader.Read(&req_copy));
  EXPECT_TRUE(reader.eof());
}

TEST(RecordIOTest, WriteAndReadFile) {
  TempDir temp_dir;

  std::filesystem::path test_file_path = temp_dir.path() / "test";

  std::ofstream ofs(test_file_path);
  HelloRequest req;
  req.set_name("foo");
  EXPECT_TRUE(SerializeToStream(req, &ofs));
  req.set_name("bar");
  EXPECT_TRUE(SerializeToStream(req, &ofs));
  ofs.close();

  std::ifstream ifs(test_file_path);
  Reader reader(&ifs);
  HelloRequest req_copy;

  EXPECT_TRUE(reader.Read(&req_copy));
  EXPECT_FALSE(reader.eof());
  EXPECT_THAT(req_copy, EqualsProto("name: 'foo'"));

  EXPECT_TRUE(reader.Read(&req_copy));
  EXPECT_FALSE(reader.eof());
  EXPECT_THAT(req_copy, EqualsProto("name: 'bar'"));

  EXPECT_FALSE(reader.Read(&req_copy));
  EXPECT_TRUE(reader.eof());
}

}  // namespace rio
}  // namespace pl
