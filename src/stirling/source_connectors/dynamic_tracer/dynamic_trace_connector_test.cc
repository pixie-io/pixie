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

#include <unistd.h>
#include <regex>

#include <gmock/gmock.h>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_trace_connector.h"

namespace px {
namespace stirling {

TEST(DynamicTraceConnectorTest, ConvertFields) {
  constexpr std::string_view kOutputStruct = R"(
      name: "out_table_value_t"
      fields {
        name: "tgid_"
        type: INT32
      }
      fields {
        name: "tgid_start_time_"
        type: UINT64
      }
      fields {
        name: "goid_"
        type: INT64
      }
      fields {
        name: "time_"
        type: UINT64
      }
      fields {
        name: "arg0"
        type: INT
      }
      fields {
        name: "arg1"
        type: BOOL
      }
      fields {
        name: "arg2"
        type: BOOL
      }
  )";

  ::px::stirling::dynamic_tracing::ir::physical::Struct output_struct;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(std::string(kOutputStruct), &output_struct));

  BackedDataElements elements = ConvertFields(output_struct.fields());

  ASSERT_EQ(elements.elements().size(), 6);
  // Check that tgid_ and tgid_start_time_ -> upid.
  EXPECT_EQ(elements.elements()[0].name(), "upid");
  EXPECT_EQ(elements.elements()[1].name(), "goid_");
  EXPECT_EQ(elements.elements()[2].name(), "time_");
  EXPECT_EQ(elements.elements()[3].name(), "arg0");
  EXPECT_EQ(elements.elements()[4].name(), "arg1");
  EXPECT_EQ(elements.elements()[5].name(), "arg2");

  // There's a hack to convert any column with name "time_" to TIME64NS. Check that.
  EXPECT_EQ(elements.elements()[2].type(), types::TIME64NS);
}

}  // namespace stirling
}  // namespace px
