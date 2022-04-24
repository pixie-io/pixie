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

#include "src/carnot/funcs/builtins/uri_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

TEST(URIOps, basic_parse_test) {
  auto udf_tester = udf::UDFTester<URIParseUDF>();

  udf_tester.ForInput("https://px.dev/community/?param1=val1&param1=val2#contributors")
      .Expect(R"({"scheme":"https","host":"px.dev","path":"community/",)"
              R"("query":"param1=val1&param1=val2","fragment":"contributors"})");

  udf_tester.ForInput(R"(grpc://127.0.0.1:9876/stream)")
      .Expect(R"({"scheme":"grpc","host":"127.0.0.1",)"
              R"("port":9876,"path":"stream"})");

  udf_tester.ForInput(R"(grpc://127.0.0.1/stream?s=test%20spaces)")
      .Expect(R"({"scheme":"grpc","host":"127.0.0.1",)"
              R"("path":"stream","query":"s=test%20spaces"})");

  udf_tester.ForInput(R"(http://127.0.0.1)").Expect(R"({"scheme":"http","host":"127.0.0.1"})");

  udf_tester.ForInput(R"(http://127.0.0.1:-1234)").Expect("Failed to parse URI");
}

TEST(URIOps, basic_recompose_test) {
  auto udf_tester = udf::UDFTester<URIRecomposeUDF>();

  udf_tester
      .ForInput("https", "", "px.dev", 0, "community/", "param1=val1&param1=val2", "contributors")
      .Expect("https://px.dev/community/?param1=val1&param1=val2#contributors");

  udf_tester.ForInput("grpc", "", "127.0.0.1", 9876, "path", "", "")
      .Expect("grpc://127.0.0.1:9876/path");

  udf_tester.ForInput("grpc", "", "127.0.0.1", 0, "", "param=query%20with%20spaces", "")
      .Expect("grpc://127.0.0.1/?param=query%20with%20spaces");

  udf_tester.ForInput("grpc", "", "127.0.0.1", -10, "path", "", "")
      .Expect("Failed to recompose URI");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
