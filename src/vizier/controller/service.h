#pragma once

#include <memory>

#include <libcuckoo/cuckoohash_map.hh>
#include <sole.hpp>

#include "src/common/common.h"
#include "src/common/uuid_utils.h"
#include "src/vizier/controller/agent_connector.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/proto/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace vizier {

/**
 * The GRPC Vizier service.
 *
 * High level diagram of how this works is below. RPC clients can communicate to
 * this service and indirectly issue queries to downstream agents,
 * which are each managed by an agent connector.
 *
 *
 *    +-------------+
 *    | RPC Clients |
 *    +-----+-------+                  Stream GRPC
 *          |                              |
 *    +-----v----+      +------------+     |   +--------------+
 *    |          |      | Agent Conn +-+   |   |              |
 *    |          +<---->>            | <-------+ Agent        |
 *    | Service  |      +------------+ |       |              |
 *    |          |        +------------+       +--------------+
 *    |          |
 *    |          |
 *    |          |
 *    +----------+
 *
 */
class VizierServiceImpl : public VizierService::Service {
  using AgentReaderWriter = ::grpc::ServerReaderWriter<VizierToAgentMessage, AgentToVizierMessage>;

 public:
  ~VizierServiceImpl() override = default;
  grpc::Status ServeAgent(::grpc::ServerContext *context, AgentReaderWriter *stream) override;

  grpc::Status GetAgentInfo(::grpc::ServerContext *, const ::pl::vizier::AgentInfoRequest *,
                            ::pl::vizier::AgentInfoResponse *response) override;

  grpc::Status ExecuteQuery(::grpc::ServerContext *, const ::pl::vizier::QueryRequest *req,
                            ::pl::vizier::VizierQueryResponse *resp) override;

 private:
  cuckoohash_map<sole::uuid, std::unique_ptr<AgentConnector>> agents_;
};

}  // namespace vizier
}  // namespace pl
