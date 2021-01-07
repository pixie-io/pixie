#include <sys/socket.h>

#include <algorithm>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <benchmark/benchmark.h>

#include "src/common/base/base.h"
#include "src/stirling/socket_tracer/connection_tracker.h"

using pl::stirling::ConnectionTracker;

uint32_t kTimesteps = 1000;
uint32_t kGenerations = 5;

// This benchmark compares different implementations of the two-level map
// used to hold all connection trackers.
// In stirling, outer map's key is PID+FD, while inner map's key is generation number.
// After every iteration, there may be multiple trackers per outer map key,
// but only the latest generation will be active, and the rest should be culled.

// Choose one of the defines below to select inner map implementation.
#define INNER_MAP
// #define INNER_PRIQUEUE

//----------------------------------------------------
// Structures
//----------------------------------------------------

class ConnectionTrackerCompare {
 public:
  bool operator()(const ConnectionTracker& a, const ConnectionTracker& b) const {
    return a.conn_id().tsid > b.conn_id().tsid;
  }
};

#ifdef INNER_MAP
using InnerContainer = std::map<uint32_t, ConnectionTracker>;
#endif

#ifdef INNER_PRIQUEUE
using InnerContainer = std::priority_queue<ConnectionTracker, std::vector<ConnectionTracker>,
                                           ConnectionTrackerCompare>;
#endif

//----------------------------------------------------
// Helper functions
//----------------------------------------------------

uint32_t global_gen_num = 0;

std::vector<uint32_t> GenerateRandomSequence(size_t n, size_t c) {
  std::vector<uint32_t> values;
  values.resize(n);

  std::default_random_engine rng;
  std::uniform_int_distribution<uint32_t> dist(0, c);

  for (uint32_t i = 0; i < n; ++i) {
    values[i] = dist(rng);
  }

  return values;
}

template <typename Tcontainer>
void AddConnectionTrackers(Tcontainer* connection_trackers, const std::vector<uint32_t>& values) {
  struct socket_control_event_t conn_event {};
  conn_event.type = kConnOpen;
  conn_event.open.addr.sin6_family = AF_INET;

  for (size_t i = 0; i < values.size(); ++i) {
    uint32_t conn_id = values[i];
    uint32_t gen_num = global_gen_num++;
    auto& container = (*connection_trackers)[conn_id];
    auto tracker = ConnectionTracker();
    conn_event.open.conn_id.tsid = gen_num;
    tracker.AddControlEvent(conn_event);
#if defined(INNER_MAP)
    container.emplace(gen_num, std::move(tracker));
#elif defined(INNER_PRIQUEUE)
    container.push(tracker);
#endif
  }
}

template <typename Tcontainer>
void PruneConnectionTrackers(Tcontainer* connection_trackers) {
  for (auto& [key, tracker_set] : *connection_trackers) {
    PL_UNUSED(key);
#if defined(INNER_MAP)
#ifdef MOVE
    auto& last_tracker = tracker_set.rbegin()->second;
    auto new_tracker_set = InnerContainer();
    new_tracker_set.emplace(last_tracker.tsid(), std::move(last_tracker));
    tracker_set = std::move(new_tracker_set);
#else
    auto gen_it = tracker_set.begin();
    while (gen_it != tracker_set.end()) {
      bool delete_tracker = false;
      if (gen_it != --tracker_set.end()) {
        delete_tracker = true;
      }
      gen_it = delete_tracker ? tracker_set.erase(gen_it) : ++gen_it;
    }
#endif
#elif defined(INNER_PRIQUEUE)
#ifdef MOVE
    auto& last_tracker = tracker_set.top();
    auto new_tracker_set = InnerContainer();
    new_tracker_set.emplace(std::move(last_tracker));
    tracker_set = std::move(new_tracker_set);
#else
    while (tracker_set.size() > 1) {
      tracker_set.pop();
    }
#endif
#endif

    DCHECK_EQ(tracker_set.size(), 1ULL);
  }
}

template <typename Tcontainer>
// NOLINTNEXTLINE : runtime/references.
static void BM_connection_tracker_container(benchmark::State& state) {
  size_t num_connections = state.range(0);

  std::vector<uint32_t> values =
      GenerateRandomSequence(num_connections, num_connections / kGenerations);

  Tcontainer connection_trackers;

  for (auto _ : state) {
    PL_UNUSED(_);

    for (uint32_t i = 0; i < kTimesteps; ++i) {
      AddConnectionTrackers(&connection_trackers, values);
      PruneConnectionTrackers(&connection_trackers);
    }
    benchmark::DoNotOptimize(connection_trackers);
    connection_trackers.clear();
  }

  state.SetItemsProcessed(kTimesteps * num_connections);
}

BENCHMARK_TEMPLATE(BM_connection_tracker_container, std::map<uint64_t, InnerContainer>)
    ->RangeMultiplier(10)
    ->Range(100, 10000);
BENCHMARK_TEMPLATE(BM_connection_tracker_container, std::unordered_map<uint64_t, InnerContainer>)
    ->RangeMultiplier(10)
    ->Range(100, 10000);
BENCHMARK_TEMPLATE(BM_connection_tracker_container, absl::flat_hash_map<uint64_t, InnerContainer>)
    ->RangeMultiplier(10)
    ->Range(100, 10000);
BENCHMARK_TEMPLATE(BM_connection_tracker_container, absl::node_hash_map<uint64_t, InnerContainer>)
    ->RangeMultiplier(10)
    ->Range(100, 10000);
