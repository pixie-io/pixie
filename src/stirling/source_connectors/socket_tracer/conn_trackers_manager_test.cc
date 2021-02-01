#include <random>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"

namespace pl {
namespace stirling {

class ConnTrackersManagerTest : public ::testing::Test {
 protected:
  ConnTrackersManagerTest() : rng_(37), probability_dist_(0.0, 1.0) {}

  ConnTrackersManager trackers_;

  std::default_random_engine rng_;
  std::uniform_real_distribution<double> probability_dist_;

  void CleanupTrackers() {
    VLOG(1) << "CleanupTrackers";
    trackers_.CleanupTrackers();
  }

  void TransferStreamsProxy(TrafficProtocol protocol, double mark_for_death_probabilty,
                            int death_countdown) {
    VLOG(1) << absl::Substitute("TransferStreamsProxy $0 $1", magic_enum::enum_name(protocol),
                                mark_for_death_probabilty);
    ConnTrackersManager::TrackersList conn_trackers_list =
        trackers_.ConnTrackersForProtocol(protocol);

    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnectionTracker* tracker = *iter;
      if (probability_dist_(rng_) < mark_for_death_probabilty) {
        tracker->MarkForDeath(death_countdown);
      }
    }
  }

  void TrackerEvent(struct conn_id_t conn_id, TrafficProtocol protocol) {
    VLOG(1) << "TrackerEvent";
    ConnectionTracker& tracker = trackers_.GetOrCreateConnTracker(conn_id);
    tracker.SetConnID(conn_id);
    tracker.SetProtocol(protocol);
  }
};

// This is a stress on ConnTrackersManager.
// Each iteration, a different action is taken, and the consistency of the structure is checked.
// ASAN runs can also identify issues while being stressed.
TEST_F(ConnTrackersManagerTest, Fuzz) {
  constexpr int kIters = 1000000;
  std::uniform_int_distribution<int> death_countdown_dist(0, 5);
  std::uniform_int_distribution<int> pid_dist(1, 5);
  std::uniform_int_distribution<int> tsid_dist(1, 2);
  std::uniform_int_distribution<int> protocol_dist(0, kNumProtocols - 1);

  for (int i = 0; i < kIters; ++i) {
    auto protocol = magic_enum::enum_cast<TrafficProtocol>(protocol_dist(rng_));
    CHECK(protocol.has_value());

    // Randomly pick an action to take.
    //  1) TrackerEvent: proxy of a single BPF event from PollPerfBuffers that could change the
    //  ConnectionTracker state (including protocol). 2) TransferStreamsProxy: proxy of
    //  TransferStreams(), which processes all the ConnectionTrackers for a given protocol.
    //      - This can result in a tracker getting into the ReadyForDestruction state.
    //  3) CleanupTrackers: proxy of CleanupTrackers(), which runs periodically in Stirling.
    // This is a proxy of what happens in a real Stirling implementation.
    double x = probability_dist_(rng_);
    if (x < 0.80) {
      uint32_t pid = pid_dist(rng_);
      uint32_t fd = 1;
      uint64_t tsid = tsid_dist(rng_);

      struct conn_id_t conn_id = {{{pid}, 0}, fd, tsid};
      TrackerEvent(conn_id, protocol.value());
    } else if (x < 0.95) {
      int death_countdown = death_countdown_dist(rng_);
      double mark_for_death_prob = probability_dist_(rng_);
      TransferStreamsProxy(protocol.value(), mark_for_death_prob, death_countdown);
    } else {
      CleanupTrackers();
    }

    ASSERT_OK(trackers_.TestOnlyCheckConsistency());
  }
}

// This test case is inspired from an elusive bug that caused memory corruption issues due to
// heap-use-after-free. It was caught by the Fuzz test and boiled down to a simple sequence here.
// Now that we don't allow a tracker to be in multiple lists, it is less likely to trigger.
TEST_F(ConnTrackersManagerTest, ChangeProtocolsWhileReadyForDestruction) {
  struct conn_id_t conn_id = {{{5}, 0}, 1, 12345};
  constexpr int kDeathCountdown = 0;
  constexpr double kMarkForDeathProb = 1.0;

  LOG(INFO) << "Add a new event into unknown protocol list.";
  TrackerEvent(conn_id, kProtocolUnknown);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after adding new event with kProtocolUnknown.";

  LOG(INFO) << "Make tracker ReadyForDestruction by processing the unknown protocols list.";
  TransferStreamsProxy(kProtocolUnknown, kMarkForDeathProb, kDeathCountdown);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after TransferStreams on kProtocolUnknown.";

  LOG(INFO) << "Process HTTP protocols list. This should have no effect.";
  TransferStreamsProxy(kProtocolHTTP, kMarkForDeathProb, kDeathCountdown);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after TransferStreams on kProtocolHTTP.";

  LOG(INFO) << "A new event moves the tracker to the HTTP list.";
  TrackerEvent(conn_id, kProtocolHTTP);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after updating tracker to kProtocolHTTP.";

  LOG(INFO) << "Process unknown protocols list.";
  TransferStreamsProxy(kProtocolUnknown, kMarkForDeathProb, kDeathCountdown);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after TransferStreams on kProtocolUnknown.";

  LOG(INFO) << "CleanupTrackers. The tracker is removed, so it better not be in any lists.";
  CleanupTrackers();
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency()) << "Inconsistent state after CleanupTrackers.";

  LOG(INFO) << "Process HTTP protocols list.";
  TransferStreamsProxy(kProtocolHTTP, kMarkForDeathProb, kDeathCountdown);
  LOG(INFO) << trackers_.DebugInfo();
  ASSERT_OK(trackers_.TestOnlyCheckConsistency())
      << "Inconsistent state after TransferStreams on kProtocolHTTP.";
}

}  // namespace stirling
}  // namespace pl
