#pragma once

#include <arrow/memory_pool.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/exec/exec_state.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/queryresultspb/query_results.pb.h"
#include "src/common/base/base.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/table_store/table_store.h"

namespace px {
namespace carnot {

constexpr auto kRPCResultTimeout = std::chrono::seconds(2);

class Carnot : public NotCopyable {
 public:
  static StatusOr<std::unique_ptr<Carnot>> Create(
      const sole::uuid& agent_id, std::unique_ptr<udf::Registry> func_registry,
      std::shared_ptr<table_store::TableStore> table_store,
      const exec::ResultSinkStubGenerator& stub_generator,
      std::function<void(grpc::ClientContext* ctx)> add_auth_to_grpc_context_func,
      int grpc_server_port, std::shared_ptr<grpc::ServerCredentials> grpc_server_creds);

  static StatusOr<std::unique_ptr<Carnot>> Create(
      const sole::uuid& agent_id, std::shared_ptr<table_store::TableStore> table_store,
      const exec::ResultSinkStubGenerator& stub_generator, int grpc_server_port = 0,
      std::shared_ptr<grpc::ServerCredentials> grpc_server_creds = nullptr);

  using AgentMetadataCallbackFunc = std::function<std::shared_ptr<const md::AgentMetadataState>()>;

  virtual ~Carnot() = default;

  /**
   * Executes the given query.
   *
   * @param query the query in the form of a string.
   * @param time_now the current time.
   * @return a Carnot Return with output_tables if successful. Error status otherwise.
   */
  virtual Status ExecuteQuery(const std::string& query, const sole::uuid& query_id,
                              types::Time64NSValue time_now, bool analyze = false) = 0;
  /**
   * Executes the given logical plan.
   *
   * @param plan the plan protobuf describing what should be compiled.
   * @return a Carnot Return with output_tables if successful. Error status otherwise.
   */
  virtual Status ExecutePlan(const planpb::Plan& plan, const sole::uuid& query_id,
                             bool analyze = false) = 0;

  /**
   * Registers the callback for updating the agents metadata state.
   */
  virtual void RegisterAgentMetadataCallback(AgentMetadataCallbackFunc func) = 0;

  /**
   * Returns a const pointer to carnot's function registry.
   */
  virtual const udf::Registry* FuncRegistry() const = 0;
};

}  // namespace carnot
}  // namespace px
