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

#include <string>

#include "src/carnot/funcs/net/net_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace funcs {
namespace net {

TEST(NetOps, CIDRsContainIPUDF_basic) {
  auto udf_tester = px::carnot::udf::UDFTester<CIDRsContainIPUDF>();
  std::string cidrs(R"(["10.0.0.1/31"])");
  udf_tester.ForInput(cidrs, "10.0.0.1").Expect(true);
  udf_tester.ForInput(cidrs, "10.0.0.0").Expect(true);
  udf_tester.ForInput(cidrs, "10.0.0.2").Expect(false);
}

TEST(NetOps, CIDRsContainIPUDF_invalid_json) {
  auto udf_tester = px::carnot::udf::UDFTester<CIDRsContainIPUDF>();

  // Check that various kinds of invalid json don't cause failures and just return false.
  udf_tester.ForInput(R"({"test":123})", "10.0.0.1").Expect(false);
  udf_tester.ForInput(R"([1, 3])", "10.0.0.1").Expect(false);
  udf_tester.ForInput(R"([{"1":1}])", "10.0.0.1").Expect(false);
  udf_tester.ForInput(R"(;{})", "10.0.0.1").Expect(false);
}

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace px
