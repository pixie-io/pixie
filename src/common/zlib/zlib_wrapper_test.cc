#include "src/common/zlib/zlib_wrapper.h"
#include <gtest/gtest.h>
#include <zlib.h>
#include <string>

namespace pl {

class ZlibTest : public ::testing::Test {
 private:
  inline static const uint8_t compressed_str_bytes_[] = {
      0x1f, 0x8b, 0x08, 0x00, 0x37, 0xf0, 0xbf, 0x5c, 0x00, 0x03, 0x0b,
      0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44, 0x85, 0x92, 0xd4, 0xe2,
      0x12, 0x2e, 0x00, 0x8c, 0x2d, 0xc0, 0xfa, 0x0f, 0x00, 0x00, 0x00};
  inline static const std::string expected_result_ = "This is a test\n";

 public:
  std::string GetCompressedString() {
    return std::string(reinterpret_cast<const char*>(compressed_str_bytes_),
                       sizeof(compressed_str_bytes_));
  }

  std::string GetExpectedResult() { return expected_result_; }
};

TEST_F(ZlibTest, inflate_test) {
  char decompress_buffer[100];
  std::string compressed_str = GetCompressedString();
  auto err =
      pl::zlib::Inflate(compressed_str.c_str(), compressed_str.size(), decompress_buffer, 100);
  ASSERT_EQ(Z_STREAM_END, err);
  EXPECT_EQ(decompress_buffer, GetExpectedResult());
}

TEST_F(ZlibTest, str_inflate_test) {
  auto result = pl::zlib::StrInflate(GetCompressedString());
  EXPECT_OK(result);
  EXPECT_EQ(result.ValueOrDie(), GetExpectedResult());
}

}  // namespace pl
