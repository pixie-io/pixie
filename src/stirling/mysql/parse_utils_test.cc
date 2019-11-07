#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <utility>

#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql/mysql_handler.h"

namespace pl {
namespace stirling {
namespace mysql {

// TODO(oazizi): Move ProcessLengthEncodedInt tests to a different file.

TEST(ProcessLengthEncodedInt, Basic) {
  int64_t num;
  size_t offset;

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\x12"), &offset).ValueOrDie();
  EXPECT_EQ(num, 18);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfa"), &offset).ValueOrDie();
  EXPECT_EQ(num, 250);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfc\xfb\x00"), &offset).ValueOrDie();
  EXPECT_EQ(num, 251);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23\x45"), &offset).ValueOrDie();
  EXPECT_EQ(num, 0x452301);

  offset = 0;
  num = ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset)
            .ValueOrDie();
  EXPECT_EQ(num, 0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, WithOffset) {
  int64_t num;
  size_t offset;

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\x12"), &offset).ValueOrDie();
  EXPECT_EQ(num, 18);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfa"), &offset).ValueOrDie();
  EXPECT_EQ(num, 250);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfc\xfb\x00"), &offset).ValueOrDie();
  EXPECT_EQ(num, 251);

  offset = 1;
  num = ProcessLengthEncodedInt(ConstStringView("\x77\xfd\x01\x23\x45"), &offset).ValueOrDie();
  EXPECT_EQ(num, 0x452301);

  offset = 1;
  num =
      ProcessLengthEncodedInt(ConstStringView("\x77\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset)
          .ValueOrDie();
  EXPECT_EQ(num, 0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, NotEnoughBytes) {
  StatusOr<int64_t> s;
  size_t offset;

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView(""), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfc\xfb"), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23"), &offset);
  EXPECT_NOT_OK(s);

  offset = 0;
  s = ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd"), &offset);
  EXPECT_NOT_OK(s);
}

TEST(DissectParams, Basics) {
  ParamPacket p;
  Status s;
  size_t offset;

  // Length-encoded strings:

  offset = 0;
  s = DissectStringParam(ConstStringView("\x05"
                                         "mysql"),
                         &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "mysql");
  EXPECT_EQ(offset, 6);

  // Fixed-length integers:

  offset = 0;
  s = DissectIntParam<1>(ConstStringView("\x05"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "5");
  EXPECT_EQ(offset, 1);

  offset = 0;
  s = DissectIntParam<2>(ConstStringView("\x01\x23"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "8961");
  EXPECT_EQ(offset, 2);

  offset = 0;
  s = DissectIntParam<4>(ConstStringView("\x01\x23\x45\x67"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "1732584193");
  EXPECT_EQ(offset, 4);

  offset = 0;
  s = DissectIntParam<8>(ConstStringView("\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "-1167088121787636991");
  EXPECT_EQ(offset, 8);

  // Floats and Doubles:

  offset = 0;
  s = DissectFloatParam<float>(ConstStringView("\x33\x33\x23\x41"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "10.200000");
  EXPECT_EQ(offset, 4);

  offset = 0;
  s = DissectFloatParam<double>(ConstStringView("\x66\x66\x66\x66\x66\x66\x24\x40"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "10.200000");
  EXPECT_EQ(offset, 8);

  // Dates and times (length byte prefix):

  offset = 0;
  s = DissectDateTimeParam(ConstStringView("\x0b\xda\x07\x0a\x11\x13\x1b\x1e\x01\x00\x00\x00"),
                           &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "MySQL DateTime rendering not implemented yet");
  EXPECT_EQ(offset, 12);

  offset = 0;
  s = DissectDateTimeParam(ConstStringView("\x04\xda\x07\x0a\x11"), &offset, &p);
  EXPECT_OK(s);
  EXPECT_EQ(p.value, "MySQL DateTime rendering not implemented yet");
  EXPECT_EQ(offset, 5);
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
