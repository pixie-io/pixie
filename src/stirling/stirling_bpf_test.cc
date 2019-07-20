#include <gtest/gtest.h>
#include <ctime>
#include <functional>
#include <iomanip>
#include <thread>
#include <utility>

#include "src/common/base/base.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/socket_trace_connector.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"
#include "src/stirling/types.h"

using pl::stirling::SocketTraceConnector;
using pl::stirling::SourceRegistry;
using pl::stirling::Stirling;

using pl::types::ColumnWrapperRecordBatch;

using pl::ConstVectorView;

class StirlingBPFTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>();
    registry->RegisterOrDie<SocketTraceConnector>("socket_trace_connector");

    // Make Stirling.
    stirling_ = Stirling::Create(std::move(registry));

    // Set a dummy callback function (normally this would be in the agent).
    stirling_->RegisterCallback(std::bind(&StirlingBPFTest::AppendData, this, std::placeholders::_1,
                                          std::placeholders::_2));
  }

  void AppendData(uint64_t table_id, std::unique_ptr<ColumnWrapperRecordBatch> record_batch) {
    PL_UNUSED(table_id);
    PL_UNUSED(record_batch);
    // A black hole.
  }

  std::unique_ptr<Stirling> stirling_;
};

// Stop Stirling. Meant to be called asynchronously, via a thread.
void AsyncKill(Stirling* stirling) {
  if (stirling != nullptr) {
    stirling->Stop();
  }
}

TEST_F(StirlingBPFTest, CleanupTest) {
  ASSERT_OK(stirling_->RunAsThread());

  // Wait for thread to initialize.
  // TODO(oazizi): This is not good. How do we know how much time is enough?
  std::this_thread::sleep_for(std::chrono::seconds(1));

  EXPECT_GT(SocketTraceConnector::NumAttachedProbes(), 0);
  EXPECT_GT(SocketTraceConnector::NumOpenPerfBuffers(), 0);

  std::thread killer_thread = std::thread(&AsyncKill, stirling_.get());

  ASSERT_TRUE(killer_thread.joinable());
  killer_thread.join();

  EXPECT_EQ(SocketTraceConnector::NumAttachedProbes(), 0);
  EXPECT_EQ(SocketTraceConnector::NumOpenPerfBuffers(), 0);
}
