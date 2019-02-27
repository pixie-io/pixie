#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/carnot.h"
#include "src/common/types/types.pb.h"
#include "src/stirling/proto/collector_config.pb.h"
#include "src/stirling/stirling.h"

namespace pl {
namespace agent {

using carnot::Carnot;
using stirling::Stirling;
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
   * @return Publish
   */
  Publish GeneratePublishMessage();

  /**
   * @brief Subscribe to all elements in all info classes of a publish message.
   *
   * @param publish_message
   * @return Subscribe
   */
  Subscribe SubscribeToEverything(const Publish& publish_message);

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

  Carnot* carnot() { return carnot_.get(); }
  Stirling* stirling() { return stirling_.get(); }

 private:
  std::unique_ptr<Carnot> carnot_;
  std::unique_ptr<Stirling> stirling_;
};

}  // namespace agent
}  // namespace pl
