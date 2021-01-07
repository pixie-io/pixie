#pragma once

#include <gmock/gmock.h>
#include <memory>
#include <sole.hpp>

#include "src/common/uuid/uuid.h"
#include "src/stirling/core/source_registry.h"
#include "src/stirling/dynamic_tracer/dynamic_tracing/ir/logicalpb/logical.pb.h"
#include "src/stirling/proto/stirling.pb.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace stirling {

class MockStirling : public Stirling {
 public:
  MOCK_METHOD(void, RegisterTracepoint,
              (sole::uuid trace_id,
               std::unique_ptr<dynamic_tracing::ir::logical::TracepointDeployment> program),
              (override));
  MOCK_METHOD(StatusOr<stirlingpb::Publish>, GetTracepointInfo, (sole::uuid trace_id), (override));
  MOCK_METHOD(Status, RemoveTracepoint, (sole::uuid trace_id), (override));
  MOCK_METHOD(void, GetPublishProto, (stirlingpb::Publish * publish_pb), (override));
  MOCK_METHOD(Status, SetSubscription, (const stirlingpb::Subscribe& subscribe_proto), (override));
  MOCK_METHOD(void, RegisterDataPushCallback, (DataPushCallback f), (override));
  MOCK_METHOD(void, RegisterAgentMetadataCallback, (AgentMetadataCallback f), (override));
  MOCK_METHOD(void, Run, (), (override));
  MOCK_METHOD(Status, RunAsThread, (), (override));
  MOCK_METHOD(void, WaitForThreadJoin, (), (override));
  MOCK_METHOD(void, Stop, (), (override));
};

}  // namespace stirling
}  // namespace pl
