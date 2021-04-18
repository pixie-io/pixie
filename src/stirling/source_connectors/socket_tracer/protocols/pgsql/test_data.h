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

#pragma once

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {

#define D(var, str) const std::string_view var = CreateStringView<char>(str)

D(kParseData1,
  "\x50\x00\x00\x00\x30\x00\x53\x45\x4c\x45\x43\x54\x20\x2a\x20\x46"
  "\x52\x4f\x4d\x20\x70\x65\x72\x73\x6f\x6e\x20\x57\x48\x45\x52\x45"
  "\x20\x66\x69\x72\x73\x74\x5f\x6e\x61\x6d\x65\x3d\x24\x31\x00\x00\x00");
D(kDescData, "\x44\x00\x00\x00\x06\x53\x00");
D(kBindData,
  "\x42\x00\x00\x00\x15\x00\x00\x00\x00\x00\x01\x00\x00\x00\x05\x4a"
  "\x61\x73\x6f\x6e\x00\x00");
D(kExecData, "\x45\x00\x00\x00\x09\x00\x00\x00\x00\x00");

D(kParseCmplData, "\x31\x00\x00\x00\x04");
D(kParamDescData, "\x74\x00\x00\x00\x0a\x00\x01\x00\x00\x00\x19");
D(kRowDescData,
  "\x54\x00\x00\x00\x57\x00\x03\x66\x69\x72\x73\x74\x5f\x6e\x61\x6d"
  "\x65\x00\x00\x00\x40\x00\x00\x01\x00\x00\x00\x19\xff\xff\xff\xff"
  "\xff\xff\x00\x00\x6c\x61\x73\x74\x5f\x6e\x61\x6d\x65\x00\x00\x00"
  "\x40\x00\x00\x02\x00\x00\x00\x19\xff\xff\xff\xff\xff\xff\x00\x00"
  "\x65\x6d\x61\x69\x6c\x00\x00\x00\x40\x00\x00\x03\x00\x00\x00\x19"
  "\xff\xff\xff\xff\xff\xff\x00\x00");
D(kBindCmplData, "\x32\x00\x00\x00\x04");
D(kDataRowData,
  "\x44\x00\x00\x00\x30\x00\x03\x00\x00\x00\x05\x4a\x61\x73\x6f\x6e"
  "\x00\x00\x00\x06\x4d\x6f\x69\x72\x6f\x6e\x00\x00\x00\x13\x6a\x6d"
  "\x6f\x69\x72\x6f\x6e\x40\x6a\x6d\x6f\x69\x72\x6f\x6e\x2e\x6e\x65\x74");
D(kCmdCmplData, "\x43\x00\x00\x00\x0f\x53\x45\x4c\x45\x43\x54\x20\x32\x33\x38\x00");

D(kBindUnnamedData,
  // kBind
  "B\000\000\000\040"
  // Portal
  "\000"
  // Statement
  "\000"
  // Parameter format code count
  "\000\000"
  // Number of parameter
  "\000\002"
  "\x00\x00\x00\x03"
  "foo"
  "\x00\x00\x00\x03"
  "bar"
  // Number of parameter format codes
  "\000\000"
  // Number of result column format codes
  "\000\002"
  // 2 kBinary
  "\000\001"
  "\000\001");

D(kBindNamedData,
  // kBind
  "B\000\000\000\043"
  // Portal
  "\000"
  // Statement
  "foo\000"
  // Parameter format code count
  "\000\000"
  // Number of parameter
  "\000\002"
  "\x00\x00\x00\x03"
  "foo"
  "\x00\x00\x00\x03"
  "bar"
  // Number of parameter format codes
  "\000\000"
  // Number of result column format codes
  "\000\002"
  // 2 kBinary
  "\000\001"
  "\000\001");

D(kErrRespData,
  "\x45\x00\x00\x00\x66\x53\x45\x52\x52\x4f\x52\x00\x56\x45\x52\x52"
  "\x4f\x52\x00\x43\x34\x32\x50\x30\x31\x00\x4d\x72\x65\x6c\x61\x74"
  "\x69\x6f\x6e\x20\x22\x78\x78\x78\x22\x20\x64\x6f\x65\x73\x20\x6e"
  "\x6f\x74\x20\x65\x78\x69\x73\x74\x00\x50\x31\x35\x00\x46\x70\x61"
  "\x72\x73\x65\x5f\x72\x65\x6c\x61\x74\x69\x6f\x6e\x2e\x63\x00\x4c"
  "\x31\x31\x39\x34\x00\x52\x70\x61\x72\x73\x65\x72\x4f\x70\x65\x6e"
  "\x54\x61\x62\x6c\x65\x00\x00");

D(kRollbackMsg, "\x51\x00\x00\x00\x0d\x52\x4f\x4c\x4c\x42\x41\x43\x4b\x00");
D(kRollbackCmplMsg, "\x43\x00\x00\x00\x0d\x52\x4f\x4c\x4c\x42\x41\x43\x4b\x00");

D(kRowDescTestData,
  "T\000\000\000\246"
  "\000\006"
  "Name"
  "\000\000\000\004\356\000\002\000\000\000\023\000@\377\377\377\377\000\000"
  "Owner"
  "\000\000\000\000\000\000\000\000\000\000\023\000@\377\377\377\377\000\000"
  "Encoding"
  "\000\000\000\000\000\000\000\000\000\000\023\000@\377\377\377\377\000\000"
  "Collate"
  "\000\000\000\004\356\000\005\000\000\000\023\000@\377\377\377\377\000\000"
  "Ctype"
  "\000\000\000\004\356\000\006\000\000\000\023\000@\377\377\377\377\000\000"
  "Access "
  "privileges\000\000\000\000\000\000\000\000\000\000\031\377\377\377\377\377\377\000\000");

D(kDataRowTestData,
  "D"
  "\000\000\000F"
  "\000\006"
  "\000\000\000\010postgres"
  "\000\000\000\010postgres"
  "\000\000\000\004UTF8"
  "\000\000\000\nen_US.utf8"
  "\000\000\000\nen_US.utf8"
  "\377\377\377\377");

D(kSelectQueryMsg,
  "Q\x00\x00\x00\x19"
  "select * from table;\x00");

D(kDropTableQueryMsg,
  "Q\x00\x00\x00\x14"
  "drop table foo;\x00");

D(kDropTableCmplMsg, "C\000\000\000\017DROP TABLE\000");

}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
