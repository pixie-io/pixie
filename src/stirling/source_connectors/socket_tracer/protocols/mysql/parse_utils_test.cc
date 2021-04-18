/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/stirling/source_connectors/socket_tracer/protocols/mysql/parse_utils.h"
#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

TEST(ProcessLengthEncodedInt, Basic) {
  size_t offset;

  offset = 0;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\x12"), &offset), 18);

  offset = 0;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\xfa"), &offset), 250);

  offset = 0;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\xfc\xfb\x00"), &offset), 251);

  offset = 0;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23\x45"), &offset), 0x452301);

  offset = 0;
  EXPECT_OK_AND_EQ(
      ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset),
      0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, WithOffset) {
  size_t offset;

  offset = 1;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\x77\x12"), &offset), 18);

  offset = 1;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\x77\xfa"), &offset), 250);

  offset = 1;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\x77\xfc\xfb\x00"), &offset), 251);

  offset = 1;
  EXPECT_OK_AND_EQ(ProcessLengthEncodedInt(ConstStringView("\x77\xfd\x01\x23\x45"), &offset),
                   0x452301);

  offset = 1;
  EXPECT_OK_AND_EQ(
      ProcessLengthEncodedInt(ConstStringView("\x77\xfe\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset),
      0xefcdab8967452301);
}

TEST(ProcessLengthEncodedInt, NotEnoughBytes) {
  size_t offset;

  offset = 0;
  EXPECT_NOT_OK(ProcessLengthEncodedInt(ConstStringView(""), &offset));

  offset = 0;
  EXPECT_NOT_OK(ProcessLengthEncodedInt(ConstStringView("\xfc\xfb"), &offset));

  offset = 0;
  EXPECT_NOT_OK(ProcessLengthEncodedInt(ConstStringView("\xfd\x01\x23"), &offset));

  offset = 0;
  EXPECT_NOT_OK(
      ProcessLengthEncodedInt(ConstStringView("\xfe\x01\x23\x45\x67\x89\xab\xcd"), &offset));
}

TEST(ProcessStmtExecuteParams, Basics) {
  std::string value;
  size_t offset;

  // Length-encoded strings:

  offset = 0;
  EXPECT_OK(DissectStringParam(ConstStringView("\x05"
                                               "mysql"),
                               &offset, &value));
  EXPECT_EQ(value, "mysql");
  EXPECT_EQ(offset, 6);

  // Fixed-length integers:

  offset = 0;
  EXPECT_OK(DissectIntParam<1>(ConstStringView("\x05"), &offset, &value));
  EXPECT_EQ(value, "5");
  EXPECT_EQ(offset, 1);

  offset = 0;
  EXPECT_OK(DissectIntParam<2>(ConstStringView("\x01\x23"), &offset, &value));
  EXPECT_EQ(value, "8961");
  EXPECT_EQ(offset, 2);

  offset = 0;
  EXPECT_OK(DissectIntParam<4>(ConstStringView("\x01\x23\x45\x67"), &offset, &value));
  EXPECT_EQ(value, "1732584193");
  EXPECT_EQ(offset, 4);

  offset = 0;
  EXPECT_OK(
      DissectIntParam<8>(ConstStringView("\x01\x23\x45\x67\x89\xab\xcd\xef"), &offset, &value));
  EXPECT_EQ(value, "-1167088121787636991");
  EXPECT_EQ(offset, 8);

  // Floats and Doubles:

  offset = 0;
  EXPECT_OK(DissectFloatParam<float>(ConstStringView("\x33\x33\x23\x41"), &offset, &value));
  EXPECT_EQ(value, "10.200000");
  EXPECT_EQ(offset, 4);

  offset = 0;
  EXPECT_OK(DissectFloatParam<double>(ConstStringView("\x66\x66\x66\x66\x66\x66\x24\x40"), &offset,
                                      &value));
  EXPECT_EQ(value, "10.200000");
  EXPECT_EQ(offset, 8);

  // Dates and times (length byte prefix):

  offset = 0;
  EXPECT_OK(DissectDateTimeParam(
      ConstStringView("\x0b\xda\x07\x0a\x11\x13\x1b\x1e\x01\x00\x00\x00"), &offset, &value));
  EXPECT_EQ(value, "MySQL DateTime rendering not implemented yet");
  EXPECT_EQ(offset, 12);

  offset = 0;
  EXPECT_OK(DissectDateTimeParam(ConstStringView("\x04\xda\x07\x0a\x11"), &offset, &value));
  EXPECT_EQ(value, "MySQL DateTime rendering not implemented yet");
  EXPECT_EQ(offset, 5);
}

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
