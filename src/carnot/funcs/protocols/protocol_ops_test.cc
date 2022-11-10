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

#include <gtest/gtest.h>

#include "src/carnot/funcs/protocols/protocol_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {

TEST(ProtocolOps, ProtocolNameUDF) {
  auto udf_tester = udf::UDFTester<ProtocolNameUDF>();
  udf_tester.ForInput(0).Expect("Unknown");
  udf_tester.ForInput(1).Expect("HTTP");
  udf_tester.ForInput(10).Expect("Kafka");
  udf_tester.ForInput(9999).Expect("Invalid (9999)");
}

TEST(ProtocolOps, HTTPRespMessageUDF) {
  auto udf_tester = udf::UDFTester<HTTPRespMessageUDF>();
  udf_tester.ForInput(400).Expect("Bad Request");
  udf_tester.ForInput(0).Expect("Unassigned");
}

TEST(ProtocolOps, KafkaAPIKeyNameUDF) {
  auto udf_tester = udf::UDFTester<KafkaAPIKeyNameUDF>();
  udf_tester.ForInput(0).Expect("Produce");
  udf_tester.ForInput(1).Expect("Fetch");
  udf_tester.ForInput(9999).Expect("9999");
}

TEST(ProtocolOps, MySQLCommandNameUDF) {
  auto udf_tester = udf::UDFTester<MySQLCommandNameUDF>();
  udf_tester.ForInput(3).Expect("Query");
  udf_tester.ForInput(0x17).Expect("StmtExecute");
  udf_tester.ForInput(9999).Expect("9999");
}

TEST(ProtocolOps, CQLOpcodeNameUDF) {
  auto udf_tester = udf::UDFTester<CQLOpcodeNameUDF>();
  udf_tester.ForInput(7).Expect("Query");
  udf_tester.ForInput(0x05).Expect("Options");
  udf_tester.ForInput(9999).Expect("9999");
}

TEST(ProtocolOps, AMQPFrameTypeUDF) {
  auto udf_tester = udf::UDFTester<AMQPFrameTypeUDF>();
  udf_tester.ForInput(1).Expect("Frame method");
  udf_tester.ForInput(2).Expect("Content Header");
  udf_tester.ForInput(8).Expect("Heartbeat");
}

TEST(ProtocolOps, AMQPMethodTypeUDF) {
  auto udf_tester = udf::UDFTester<AMQPMethodTypeUDF>();
  udf_tester.ForInput(60, 40).Expect("BasicPublish");
}

TEST(ProtocolOps, MuxFrameTypeUDF) {
  auto udf_tester = udf::UDFTester<MuxFrameTypeUDF>();
  udf_tester.ForInput(1).Expect("Treq");
  udf_tester.ForInput(-1).Expect("Rreq");
  udf_tester.ForInput(68).Expect("Tinit");
  udf_tester.ForInput(-68).Expect("Rinit");
}

TEST(ProtocolOps, DNSRcodeNameUDF) {
  auto udf_tester = udf::UDFTester<DNSRcodeNameUDF>();
  udf_tester.ForInput(0).Expect("NOERROR");
  udf_tester.ForInput(3).Expect("NXDOMAIN");
  udf_tester.ForInput(9999).Expect("9999");
}

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
