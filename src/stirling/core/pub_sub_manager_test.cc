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

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include <cstring>
#include <utility>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/core/pub_sub_manager.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

using ::google::protobuf::TextFormat;
using ::google::protobuf::util::MessageDifferencer;
using stirlingpb::InfoClass;
using types::DataType;
using types::PatternType;
using types::SemanticType;

const char* kInfoClass0 = R"(
  schema {
    name: "cpu"
    desc: "CPU usage metrics"
    elements {
      name: "user_percentage"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: "User percentage"
    }
    elements {
      name: "system_percentage"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: "System percentage"
    }
    elements {
      name: "io_percentage"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: "IO percentage"
    }
    tabletized: false
    tabletization_key: 18446744073709551615
  }
)";

const char* kInfoClass1 = R"(
  schema {
    name: "my_table"
    desc: "Mine, mine, mine!"
    elements {
      name: "a"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: ""
    }
    elements {
      name: "b"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: ""
    }
    elements {
      name: "c"
      type: FLOAT64
      stype: ST_NONE
      ptype: METRIC_GAUGE
      desc: ""
    }
    tabletized: false
    tabletization_key: 18446744073709551615
  }
)";

// A test source connector to be used for testing.
class TestSourceConnector : public SourceConnector {
 public:
  static constexpr DataElement kElements[] = {
      {"user_percentage", "User percentage", DataType::FLOAT64, SemanticType::ST_NONE,
       PatternType::METRIC_GAUGE},
      {"system_percentage", "System percentage", DataType::FLOAT64, SemanticType::ST_NONE,
       PatternType::METRIC_GAUGE},
      {"io_percentage", "IO percentage", DataType::FLOAT64, SemanticType::ST_NONE,
       PatternType::METRIC_GAUGE}};

  static constexpr auto kTable = DataTableSchema("cpu", "CPU usage metrics", kElements);
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new TestSourceConnector(name));
  }

  Status InitImpl() override { return Status::OK(); }
  Status StopImpl() override { return Status::OK(); }
  void TransferDataImpl(ConnectorContext* /* ctx */) override{};

 protected:
  explicit TestSourceConnector(std::string_view name) : SourceConnector(name, kTables) {}
};

// A test source connector to be used for testing.
class TestSourceConnector2 : public SourceConnector {
 public:
  static constexpr DataElement kElements[] = {
      {"a", "", DataType::FLOAT64, SemanticType::ST_NONE, PatternType::METRIC_GAUGE},
      {"b", "", DataType::FLOAT64, SemanticType::ST_NONE, PatternType::METRIC_GAUGE},
      {"c", "", DataType::FLOAT64, SemanticType::ST_NONE, PatternType::METRIC_GAUGE}};

  static constexpr auto kTable = DataTableSchema("my_table", "Mine, mine, mine!", kElements);
  static constexpr auto kTables = MakeArray(kTable);

  static std::unique_ptr<SourceConnector> Create(std::string_view name) {
    return std::unique_ptr<SourceConnector>(new TestSourceConnector2(name));
  }

  Status InitImpl() override { return Status::OK(); }
  Status StopImpl() override { return Status::OK(); }
  void TransferDataImpl(ConnectorContext* /* ctx */) override{};

 protected:
  explicit TestSourceConnector2(std::string_view name) : SourceConnector(name, kTables) {}
};

class PubSubManagerTest : public ::testing::Test {
 protected:
  PubSubManagerTest() = default;
  void SetUp() override {
    {
      std::string name = "source0";
      std::unique_ptr<SourceConnector> source = TestSourceConnector::Create(name);
      auto info_class_mgr = std::make_unique<InfoClassManager>(TestSourceConnector::kTable);
      info_class_mgr->SetSourceConnector(source.get());
      info_class_mgrs_.push_back(std::move(info_class_mgr));
      sources_.push_back(std::move(source));
    }

    {
      std::string name = "source1";
      std::unique_ptr<SourceConnector> source = TestSourceConnector2::Create(name);
      auto info_class_mgr = std::make_unique<InfoClassManager>(TestSourceConnector2::kTable);
      info_class_mgr->SetSourceConnector(source.get());
      info_class_mgrs_.push_back(std::move(info_class_mgr));
      sources_.push_back(std::move(source));
    }
  }
  std::vector<std::unique_ptr<SourceConnector>> sources_;
  InfoClassManagerVec info_class_mgrs_;
};

// This test validates that the Publish proto generated by the PubSubManager
// matches the expected Publish proto message (based on kInfoClass proto
// and with some fields added in the test).
TEST_F(PubSubManagerTest, publish_test) {
  // Publish info classes using proto message.
  stirlingpb::Publish actual_publish_pb;
  PopulatePublishProto(&actual_publish_pb, info_class_mgrs_);

  // Set expectations for the publish message.
  stirlingpb::Publish expected_publish_pb;
  auto* info_class = expected_publish_pb.add_published_info_classes();
  ASSERT_TRUE(TextFormat::MergeFromString(kInfoClass0, info_class));
  info_class->set_id(0);

  info_class = expected_publish_pb.add_published_info_classes();
  ASSERT_TRUE(TextFormat::MergeFromString(kInfoClass1, info_class));
  info_class->set_id(1);

  EXPECT_TRUE(MessageDifferencer::Equals(actual_publish_pb, expected_publish_pb));
}

TEST_F(PubSubManagerTest, partial_publish_test) {
  // Publish info classes using proto message.
  stirlingpb::Publish actual_publish_pb;
  PopulatePublishProto(&actual_publish_pb, info_class_mgrs_, "cpu");

  // Set expectations for the publish message.
  stirlingpb::Publish expected_publish_pb;
  auto* info_class = expected_publish_pb.add_published_info_classes();
  ASSERT_TRUE(TextFormat::MergeFromString(kInfoClass0, info_class));

  // Copy ID from publication as the expectation.
  info_class->set_id(actual_publish_pb.published_info_classes(0).id());

  EXPECT_TRUE(MessageDifferencer::Equals(actual_publish_pb, expected_publish_pb));
}

}  // namespace stirling
}  // namespace px
