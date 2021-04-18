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
#include <string>
#include <vector>

#include "src/carnot/exec/exec_node.h"

namespace px {
namespace carnot {
namespace exec {

class MockExecNode : public ExecNode {
 public:
  MockExecNode() : ExecNode(ExecNodeType::kProcessingNode) {}
  explicit MockExecNode(const ExecNodeType& exec_node_type) : ExecNode(exec_node_type) {}

  MOCK_METHOD0(DebugStringImpl, std::string());
  MOCK_METHOD1(InitImpl, Status(const plan::Operator& plan_node));
  MOCK_METHOD1(PrepareImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(OpenImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(CloseImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(GenerateNextImpl, Status(ExecState*));
  MOCK_METHOD3(ConsumeNextImpl, Status(ExecState*, const table_store::schema::RowBatch&, size_t));
};

class MockSourceNode : public SourceNode {
 public:
  explicit MockSourceNode(const table_store::schema::RowDescriptor& output_descriptor)
      : SourceNode() {
    output_descriptor_ = std::make_unique<table_store::schema::RowDescriptor>(output_descriptor);
  }

  MOCK_METHOD0(DebugStringImpl, std::string());
  MOCK_METHOD1(InitImpl, Status(const plan::Operator& plan_node));
  MOCK_METHOD1(PrepareImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(OpenImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(CloseImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(GenerateNextImpl, Status(ExecState*));
  MOCK_METHOD3(ConsumeNextImpl, Status(ExecState*, const table_store::schema::RowBatch&, size_t));
  MOCK_METHOD0(NextBatchReady, bool());

  void SendEOS() { sent_eos_ = true; }
};

class MockSinkNode : public SinkNode {
 public:
  MockSinkNode() : SinkNode() {}

  MOCK_METHOD0(DebugStringImpl, std::string());
  MOCK_METHOD1(InitImpl, Status(const plan::Operator& plan_node));
  MOCK_METHOD1(PrepareImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(OpenImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(CloseImpl, Status(ExecState* exec_state));
  MOCK_METHOD1(GenerateNextImpl, Status(ExecState*));
  MOCK_METHOD3(ConsumeNextImpl, Status(ExecState*, const table_store::schema::RowBatch&, size_t));
};

}  // namespace exec
}  // namespace carnot
}  // namespace px
