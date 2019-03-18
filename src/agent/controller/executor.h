#pragma once
#include <arrow/api.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/shared/types/proto/types.pb.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/stirling.h"
#include "src/vizier/proto/service.pb.h"

namespace pl {
namespace agent {

using carnot::Carnot;
using carnot::exec::RecordBatchSPtr;
using carnot::plan::Relation;
using stirling::Stirling;
using stirling::stirlingpb::InfoClass;
using stirling::stirlingpb::Publish;
using stirling::stirlingpb::Subscribe;

/**
 * @brief This class manages Carnot (execution engine) and Stirling (data collector).
 * It is responsible for sending publish messages (regarding the sources available) upstream.
 * Set up tables in both carnot and stirling. After this setup, it should be able to execute
 * queries.
 */
class Executor {
 public:
  Executor(std::unique_ptr<Carnot> carnot, std::unique_ptr<Stirling> stirling)
      : carnot_(std::move(carnot)), stirling_(std::move(stirling)) {}
  ~Executor() = default;

  /**
   * @brief Initialize Stirling and Carnot. Also register call back function
   * to transfer data from Stirling to Carnot's table store.
   *
   * @return Status
   */
  Status Init();

  /**
   * @brief Generate a publish message from Stirling.
   *
   * @param publish_pb Pointer to a publish proto
   */
  void GeneratePublishMessage(Publish* publish_pb);

  /**
   * @brief Subscribe to all elements in all info classes of a publish message.
   *
   * @param publish_message
   * @return Subscribe
   */
  Subscribe SubscribeToEverything(const Publish& publish_proto);

  /**
   * @brief Create Tables from Subscription proto.
   * Create tables in Stirling and Carnot so that queries can be executed.
   * The tables are set up based on a subscription message which eventually should
   * come from the controller.
   *
   * @param subscribe_proto
   * @return Status
   */
  Status CreateTablesFromSubscription(const Subscribe& subscribe_proto);

  /**
   * @brief Add a Dummy Table in the executor for testing.
   * Remove this when we don't need dummy tables.
   *
   * @return Status
   */
  Status AddDummyTable(const std::string& name, std::shared_ptr<carnot::exec::Table> table);

  /**
   * @brief Begins collection of data samples by Stirling.
   */
  Status StartCollection() {
    if (!stirling_) {
      return error::ResourceUnavailable("Stirling collector not available.");
    }
    return stirling_->RunAsThread();
  }

  /**
   * @brief Pass query to the executor, and write to a protobuf that represents query results.
   *
   * @param query : the query to call
   * @param query_resp_pb: The result protobuf, must not be nullptr.
   * @return Status of the query execution.
   */
  Status ServiceQuery(const std::string& query, pl::vizier::AgentQueryResponse* query_resp_pb);

  Carnot* carnot() { return carnot_.get(); }
  Stirling* stirling() { return stirling_.get(); }

 private:
  std::unique_ptr<Carnot> carnot_;
  std::unique_ptr<Stirling> stirling_;
  Relation InfoClassProtoToRelation(const InfoClass& info_class_proto);
};

}  // namespace agent
}  // namespace pl
