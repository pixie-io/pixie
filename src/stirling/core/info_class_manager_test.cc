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

#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"

namespace px {
namespace stirling {

using types::DataType;
using types::PatternType;

TEST(InfoClassInfoSchemaTest, infoclass_mgr_proto_getters_test) {
  InfoClassManager info_class_mgr(SeqGenConnector::kSeq0Table);
  auto source = SeqGenConnector::Create("sequences");
  info_class_mgr.SetSourceConnector(source.get());

  EXPECT_EQ(SeqGenConnector::kSeq0Table.elements().size(),
            info_class_mgr.Schema().elements().size());
  EXPECT_EQ(SeqGenConnector::kSeq0Table.name(), info_class_mgr.name());

  stirlingpb::InfoClass info_class_pb;
  info_class_pb = info_class_mgr.ToProto();
  EXPECT_EQ(SeqGenConnector::kSeq0Table.elements().size(), info_class_pb.schema().elements_size());
  EXPECT_EQ(SeqGenConnector::kSeq0Table.name(), info_class_pb.schema().name());
  EXPECT_EQ(0, info_class_pb.id());
}

}  // namespace stirling
}  // namespace px
