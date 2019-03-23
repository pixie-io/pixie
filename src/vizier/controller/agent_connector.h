#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>

#include <sole.hpp>

#include "absl/base/thread_annotations.h"
#include "src/common/base/base.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/proto/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace vizier {

/**
 * The AgentConnector is responsible for maintaining a connection to a single agent.
 * This involves holding the GRPC connection stream, handling status changes and
 * communicating with the agent.
 */
class AgentConnector {
 public:
  /// AgentReaderWriter is a stream for communicating with the agent.
  using AgentReaderWriter = ::grpc::ServerReaderWriter<VizierToAgentMessage, AgentToVizierMessage>;

  /**
   * Create a new agent connector.
   * @param agent_id The uuid of an agent.
   * @param req The connection request.
   * @param stream The communication stream.
   */
  explicit AgentConnector(const sole::uuid& agent_id, const RegisterAgentRequest& req,
                          AgentReaderWriter* stream);
  virtual ~AgentConnector() = default;

  /**
   * This is a blocking loop that waits for requests and forwards to the appropriate internal
   * handler.
   * @return Status of loop execution (when it terminates).
   */
  virtual Status RunMessageFetchLoop();

  /**
   * Update the internal agent fetch state.
   * This is part of a background process to mark agents as unhealhty if we haven't heard
   * from them in a while.
   */
  void UpdateAgentState();

  /**
   * Get the agent status and write it to the passed in proto.
   * @param agent_status The output proto. Should not be a nullptr.
   */
  void GetAgentStatus(AgentStatus* agent_status);

  /**
   * Execute a query on the agent. This is a non-blocking call.
   * @param query_id The UUID of the query.
   * @param query_str The string corresponding to the query.
   * @return Status of sending the query execute.
   */
  virtual Status ExecuteQuery(const sole::uuid& query_id, const std::string& query_str);

  /**
   * Get results from query execution.
   * This call blocks until query execution is complete.
   * @warning Currently only a single query in flight is supported.
   * @param query_id The UUID of the query.
   * @param query_response The response proto.
   * @return Status.
   */
  virtual Status GetQueryResults(const sole::uuid& query_id, AgentQueryResponse* query_response);

  bool IsHealthy() { return agent_state_ == AGENT_STATE_HEALTHY; }

  const sole::uuid& agent_id() { return agent_id_; }

 private:
  void HandleAgentHeartBeat(const HeartBeat& heartbeat);
  void HandleQueryResponse(const AgentQueryResponse& query_resp);

  AgentReaderWriter* stream_;
  sole::uuid agent_id_;
  AgentInfo agent_info_;

  std::atomic<AgentState> agent_state_ = AGENT_STATE_UNKNOWN;
  std::atomic<int64_t> last_heartbeat_ts_ = 0;

  std::mutex query_lock_;
  // These variables are protected by the query_lock_.
  std::atomic<bool> query_in_progress_ = false;
  sole::uuid current_query_id_;
  AgentQueryResponse query_response_;
  std::condition_variable query_cv_;
  // End protected by query_lock_.
};

}  // namespace vizier
}  // namespace pl
