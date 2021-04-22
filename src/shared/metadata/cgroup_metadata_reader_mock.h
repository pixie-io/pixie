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
#include <gmock/gmock.h>
#include <string>

#include <absl/container/flat_hash_set.h>
#include "src/common/system/system.h"
#include "src/shared/metadata/cgroup_metadata_reader.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace md {

class MockCGroupMetadataReader : public CGroupMetadataReader {
 public:
  MockCGroupMetadataReader() : CGroupMetadataReader(system::Config::GetInstance()) {}
  ~MockCGroupMetadataReader() override = default;

  MOCK_CONST_METHOD5(ReadPIDs, Status(PodQOSClass qos_class, std::string_view pod_id,
                                      std::string_view container_id, ContainerType container_type,
                                      absl::flat_hash_set<uint32_t>* pid_set));
  MOCK_CONST_METHOD1(ReadPIDStartTime, int64_t(uint32_t pid));
  MOCK_CONST_METHOD1(ReadPIDCmdline, std::string(uint32_t pid));
};

}  // namespace md
}  // namespace px
