#include <arrow/pretty_print.h>
#include <gflags/gflags.h>
#include <unistd.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "src/agent/controller/controller.h"
#include "src/agent/controller/throwaway_dummy_data.h"
#include "src/common/base/base.h"
#include "src/common/perf/perf.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/schema/utils.h"
namespace pl {
namespace agent {

using types::DataType;

// The maximum size of the hostname, before we get an error.
constexpr uint64_t kMaxHostnameSize = 128;

StatusOr<std::unique_ptr<Controller>> Controller::Create(
    std::shared_ptr<grpc::Channel> chan, std::unique_ptr<carnot::Carnot> carnot,
    std::unique_ptr<stirling::Stirling> stirling,
    std::shared_ptr<table_store::TableStore> table_store) {
  std::unique_ptr<Controller> controller(
      new Controller(chan, std::move(carnot), std::move(stirling), table_store));
  PL_RETURN_IF_ERROR(controller->Init());
  return controller;
}

Controller::Controller(std::shared_ptr<grpc::Channel> chan, std::unique_ptr<carnot::Carnot> carnot,
                       std::unique_ptr<stirling::Stirling> stirling,
                       std::shared_ptr<table_store::TableStore> table_store)
    : chan_(chan),
      carnot_(std::move(carnot)),
      stirling_(std::move(stirling)),
      table_store_(table_store),
      agent_id_(sole::uuid4()) {}

Status Controller::Init() {
  vizier_stub_ = vizier::VizierService::NewStub(chan_);
  char hostname[kMaxHostnameSize];
  int err = gethostname(hostname, kMaxHostnameSize);
  if (err != 0) {
    return error::Unknown("Failed to get hostname");
  }
  LOG(INFO) << "Hostname: " << hostname_;
  return Status::OK();
}

Status Controller::SendRegisterRequest(VizierReaderWriter* stream) {
  vizier::AgentToVizierMessage msg;
  auto req = msg.mutable_register_request();
  *(req->mutable_agent_id()->mutable_data()) = agent_id_.str();
  auto agent_info = req->mutable_agent_info();
  *(agent_info->mutable_agent_id()->mutable_data()) = agent_id_.str();
  auto host_info = agent_info->mutable_host_info();
  *(host_info->mutable_hostname()) = hostname_;

  stream->Write(msg);
  return Status::OK();
}

void Controller::RunHeartBeat(VizierReaderWriter* stream) const {
  while (keepAlive_) {
    // Send a heart beat on a fixed interval.
    vizier::AgentToVizierMessage msg;
    auto hb = msg.mutable_heartbeat();
    int64_t current_time = CurrentTimeNS();
    hb->set_time(current_time);
    stream->Write(msg);
    std::this_thread::sleep_for(std::chrono::seconds(kAgentHeartBeatIntervalSeconds));
  }
}

Status Controller::ExecuteQuery(const vizier::QueryRequest& req, vizier::AgentQueryResponse* resp) {
  VLOG(1) << "Executing query: "
          << absl::StrFormat("id=%s, query=%s", ParseUUID(req.query_id()).ConsumeValueOrDie().str(),
                             req.query_str());
  CHECK(resp != nullptr);
  *resp->mutable_query_id() = req.query_id();

  carnot::CarnotQueryResult ret;
  {
    ScopedTimer query_timer("query timer");
    PL_ASSIGN_OR_RETURN(ret, carnot_->ExecuteQuery(req.query_str(), CurrentTimeNS()));
  }
  PL_RETURN_IF_ERROR(ret.ToProto(resp->mutable_query_result()));

  *resp->mutable_status() = Status::OK().ToProto();

  return Status::OK();
}

Status Controller::Run() {
  // Try to connect to vizier.
  grpc_connectivity_state state = chan_->GetState(true);
  while (state != grpc_connectivity_state::GRPC_CHANNEL_READY) {
    LOG(ERROR) << "Failed to connect to Vizier";
    sleep(1);
    state = chan_->GetState(true);
  }
  LOG(INFO) << "Connected";
  grpc::ClientContext ctx;
  VizierReaderWriterSPtr stream(vizier_stub_->ServeAgent(&ctx));
  // Send the agent info.
  PL_RETURN_IF_ERROR(SendRegisterRequest(stream.get()));
  // Start the heartbeat thread.
  std::thread hb_thread(&Controller::RunHeartBeat, this, stream.get());

  vizier::VizierToAgentMessage msg;
  while (stream->Read(&msg)) {
    VLOG(1) << "Got message: " << msg.DebugString();
    switch (msg.msg_case()) {
      case vizier::VizierToAgentMessage::kHeartBeatAck:
        break;
      case vizier::VizierToAgentMessage::kQueryRequest: {
        vizier::AgentToVizierMessage output_msg;
        PL_CHECK_OK(ExecuteQuery(msg.query_request(), output_msg.mutable_query_response()));
        if (!stream->Write(output_msg)) {
          LOG(ERROR) << "Failed to send query response to Vizier";
        }
        break;
      }
      default:
        LOG(ERROR) << "Don't know how to handler message of type: " << msg.msg_case();
    }
  }
  hb_thread.join();

  auto status = stream->Finish();
  if (!status.ok()) {
    return error::Unknown(status.error_message());
  }
  return Status::OK();
}

// Temporary and to be replaced by data table from Stirling and Executor
Status Controller::AddDummyTable(const std::string& name,
                                 std::shared_ptr<table_store::Table> table) {
  table_store_->AddTable(name, table);
  return Status::OK();
}

Status Controller::InitThrowaway() {
  // Add Dummy test data.
  PL_RETURN_IF_ERROR(
      AddDummyTable("hipster_data", pl::agent::FakeHipsterTable().ConsumeValueOrDie()));

  pl::stirling::stirlingpb::Publish publish_pb;
  stirling_->GetPublishProto(&publish_pb);
  auto subscribe_pb = stirling::SubscribeToAllInfoClasses(publish_pb);
  PL_RETURN_IF_ERROR(stirling_->SetSubscription(subscribe_pb));

  // This should eventually be done by subscribe requests.
  auto relation_info_vec = ConvertSubscribePBToRelationInfo(subscribe_pb);
  for (const auto& relation_info : relation_info_vec) {
    PL_RETURN_IF_ERROR(
        table_store_->AddTable(relation_info.name, relation_info.id,
                               std::make_shared<table_store::Table>(relation_info.relation)));
  }
  return Status::OK();
}

Status Controller::Stop() {
  keepAlive_ = false;
  return Status::OK();
}

}  // namespace agent
}  // namespace pl
