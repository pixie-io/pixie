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
#include <memory>
#include <sole.hpp>

#include "src/common/uuid/uuid.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/stirling.h"

namespace px {
namespace stirling {

class MockStirling : public Stirling {
 public:
  MOCK_METHOD(void, RegisterUserDebugSignalHandlers, (int), (override));
  MOCK_METHOD(void, RegisterTracepoint,
              (sole::uuid trace_id,
               std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program),
              (override));
  MOCK_METHOD(StatusOr<stirlingpb::Publish>, GetTracepointInfo, (sole::uuid trace_id), (override));
  MOCK_METHOD(Status, RemoveTracepoint, (sole::uuid trace_id), (override));
  MOCK_METHOD(void, GetPublishProto, (stirlingpb::Publish * publish_pb), (override));
  MOCK_METHOD(void, RegisterDataPushCallback, (DataPushCallback f), (override));
  MOCK_METHOD(void, RegisterAgentMetadataCallback, (AgentMetadataCallback f), (override));
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(Status, RunAsThread, (), (override));
  MOCK_METHOD(bool, IsRunning, (), (const override));
  MOCK_METHOD(Status, WaitUntilRunning, (std::chrono::milliseconds timeout), (const override));
  MOCK_METHOD(void, WaitForThreadJoin, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

}  // namespace stirling
}  // namespace px
