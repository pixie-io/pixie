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

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "src/carnot/planner/metadata/metadata_handler.h"
#include "src/common/testing/testing.h"

namespace px {
namespace carnot {
namespace planner {

TEST(MetadataPropertyTests, types) {
  auto md_handle = MetadataHandler::Create();

  auto id_prop = md_handle->GetProperty("container_id").ConsumeValueOrDie();
  EXPECT_EQ(types::DataType::STRING, id_prop->column_type());

  auto name_prop = md_handle->GetProperty("pod_name").ConsumeValueOrDie();
  EXPECT_EQ(types::DataType::STRING, name_prop->column_type());

  auto int64_prop = md_handle->GetProperty("pid").ConsumeValueOrDie();
  EXPECT_EQ(types::DataType::INT64, int64_prop->column_type());
}

class MetadataHandlerTests : public ::testing::Test {
 protected:
  void SetUp() { md_handler = MetadataHandler::Create(); }
  std::unique_ptr<MetadataHandler> md_handler;
};

class MetadataGetPropertyTests : public MetadataHandlerTests,
                                 public ::testing::WithParamInterface<std::string> {};

TEST_P(MetadataGetPropertyTests, has_property) {
  std::string property_name = GetParam();
  EXPECT_TRUE(md_handler->HasProperty(property_name));
  auto property_status = md_handler->GetProperty(property_name);
  EXPECT_OK(property_status);
}

std::vector<std::string> metadata_strs = {
    "service_name",  "service_id",     "pod_name",        "pod_id",         "container_id",
    "deployment",    "deployment_id",  "deployment_name", "container_name", "replica_set",
    "replicaset_id", "replica_set_id", "replicaset_name", "replicaset",     "replicaset_name"};

INSTANTIATE_TEST_SUITE_P(GetPropertyTestSuites, MetadataGetPropertyTests,
                         ::testing::ValuesIn(metadata_strs));

class MetadataAliasPropertyTests
    : public MetadataHandlerTests,
      public ::testing::WithParamInterface<std::tuple<std::string, std::string>> {};

TEST_P(MetadataAliasPropertyTests, has_property) {
  std::string alias;
  std::string property_name;
  std::tie(alias, property_name) = GetParam();
  EXPECT_TRUE(md_handler->HasProperty(property_name));
  auto property_status = md_handler->GetProperty(property_name);
  EXPECT_OK(property_status);
  EXPECT_TRUE(md_handler->HasProperty(alias));
  auto alias_status = md_handler->GetProperty(alias);
  EXPECT_OK(alias_status);
  EXPECT_EQ(alias_status.ValueOrDie(), property_status.ValueOrDie());
}
std::vector<std::tuple<std::string, std::string>> alias_to_original = {
    {"service", "service_name"},
    {"pod", "pod_name"},
    {"deployment", "deployment_name"},
    {"container", "container_name"}};

INSTANTIATE_TEST_SUITE_P(AliasPropertyTestSuites, MetadataAliasPropertyTests,
                         ::testing::ValuesIn(alias_to_original));

}  // namespace planner
}  // namespace carnot
}  // namespace px
