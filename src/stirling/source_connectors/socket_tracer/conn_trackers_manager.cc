#include "src/stirling/source_connectors/socket_tracer/conn_trackers_manager.h"

namespace pl {
namespace stirling {

//-----------------------------------------------------------------------------
// ConnTrackersManager
//-----------------------------------------------------------------------------

namespace {

uint64_t GetConnMapKey(uint32_t pid, uint32_t fd) {
  return (static_cast<uint64_t>(pid) << 32) | fd;
}

}  // namespace

void ConnTrackersManager::NotifyProtocolChange(ConnectionTracker* tracker) {
  conn_trackers_by_protocol_[tracker->traffic_class().protocol].push_back(tracker);
  ++num_trackers_in_lists_;
  ++num_tracker_dups_;
  // If this tracker was already part of another protocol list,
  // we'll remove the tracker from the original protocols list during TransferStreams().
}

ConnectionTracker& ConnTrackersManager::GetOrCreateConnTracker(struct conn_id_t conn_id) {
  const uint64_t conn_map_key = GetConnMapKey(conn_id.upid.pid, conn_id.fd);
  DCHECK(conn_map_key != 0) << "Connection map key cannot be 0, pid must be wrong";

  auto& conn_trackers = connection_trackers_[conn_map_key];
  ConnectionTracker& conn_tracker = conn_trackers[conn_id.tsid];

  // If conn_trackers[conn_id.tsid] does not exist, a new one will be created in the map.
  // New trackers always have TSID == 0, while BPF should never generate a TSID of zero,
  // so we use this as a way of detecting new trackers.
  // TODO(oazizi): Find a more direct way of detecting new trackers.
  const bool new_tracker = (conn_tracker.conn_id().tsid == 0);
  if (new_tracker) {
    conn_trackers_by_protocol_[kProtocolUnknown].push_back(&conn_tracker);

    ++num_trackers_;
    ++num_trackers_in_lists_;

    // If there is a another generation for this conn map key,
    // one of them needs to be marked for death.
    if (conn_trackers.size() > 1) {
      auto last_tracker_iter = --conn_trackers.end();

      // If the inserted conn_tracker is not the last generation, then mark it for death.
      // This can happen because the events draining from the perf buffers are not ordered.
      if (last_tracker_iter->second.conn_id().tsid != 0) {
        VLOG(1) << "Marking for death because not last generation.";
        conn_tracker.MarkForDeath();
      } else {
        // New tracker was the last, so the previous last should be marked for death.
        --last_tracker_iter;
        VLOG(1) << "Marking previous generation for death.";
        last_tracker_iter->second.MarkForDeath();
      }
    }
  }

  return conn_tracker;
}

StatusOr<const ConnectionTracker*> ConnTrackersManager::GetConnectionTracker(uint32_t pid,
                                                                             uint32_t fd) const {
  const uint64_t conn_map_key = GetConnMapKey(pid, fd);

  auto tracker_set_it = connection_trackers_.find(conn_map_key);
  if (tracker_set_it == connection_trackers_.end()) {
    return error::NotFound("Could not find the tracker with pid=$0 fd=$1.", pid, fd);
  }

  const auto& tracker_generations = tracker_set_it->second;

  if (tracker_generations.empty()) {
    DCHECK(false) << "Map entry with no tracker generations should never exist. Entry should be "
                     "deleted in such cases.";
    return error::Internal("Corrupted state: map entry with no tracker generations.");
  }

  // Return last connection.
  auto tracker_it = tracker_generations.end();
  --tracker_it;

  // Don't return trackers that are about to be destroyed.
  if (tracker_it->second.ReadyForDestruction()) {
    return error::NotFound("Connection tracker with pid=$0 fd=$1 is pending destruction.", pid, fd);
  }

  return &tracker_it->second;
}

void ConnTrackersManager::CleanupTrackers() {
  // Outer loop iterates through tracker sets (keyed by PID+FD),
  // while inner loop iterates through generations of trackers for that PID+FD pair.
  auto tracker_set_it = connection_trackers_.begin();
  while (tracker_set_it != connection_trackers_.end()) {
    auto& tracker_generations = tracker_set_it->second;

    auto generation_it = tracker_generations.begin();
    while (generation_it != tracker_generations.end()) {
      auto& tracker = generation_it->second;

      // Remove any trackers that are no longer required.
      if (tracker.ReadyForDestruction()) {
        tracker_generations.erase(generation_it++);
        --num_trackers_;
        --num_trackers_ready_for_destruction_;
      } else {
        ++generation_it;
      }
    }

    if (tracker_generations.empty()) {
      connection_trackers_.erase(tracker_set_it++);
    } else {
      ++tracker_set_it;
    }
  }

  DCHECK_EQ(num_trackers_ready_for_destruction_, 0);
}

void ConnTrackersManager::CheckConsistency() {
  DCHECK_EQ(num_trackers_,
            num_trackers_in_lists_ + num_trackers_ready_for_destruction_ - num_tracker_dups_);
}

void ConnTrackersManager::DebugInfo() const {
  int count = 0;
  for (const auto& [protocol, conn_trackers_list] : conn_trackers_by_protocol_) {
    LOG(INFO) << absl::Substitute("protocol=$0 num_trackers=$1", protocol,
                                  conn_trackers_list.size());

    count += conn_trackers_list.size();

    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnectionTracker* tracker = *iter;

      LOG(INFO) << absl::Substitute("Connection conn_id=$0 protocol=$1 state=$2\n",
                                    ToString(tracker->conn_id()),
                                    magic_enum::enum_name(tracker->traffic_class().protocol),
                                    magic_enum::enum_name(tracker->state()));
    }
  }

  LOG(INFO) << absl::Substitute("num_trackers=$0 num_trackers_allocated=$1", count, num_trackers_);
}

//-----------------------------------------------------------------------------
// TrackersListIterator
//-----------------------------------------------------------------------------

ConnTrackersManager::TrackersList::TrackersListIterator::TrackersListIterator(
    std::list<ConnectionTracker*>* trackers, std::list<ConnectionTracker*>::iterator iter,
    TrafficProtocol protocol, ConnTrackersManager* conn_trackers_manager)
    : trackers_(trackers),
      iter_(iter),
      protocol_(protocol),
      conn_trackers_manager_(conn_trackers_manager) {
  AdvanceToValidTracker();
}

ConnTrackersManager::TrackersList::TrackersListIterator
ConnTrackersManager::TrackersList::TrackersListIterator::operator++() {
  if ((*iter_)->ReadyForDestruction()) {
    trackers_->erase(iter_++);
    --conn_trackers_manager_->num_trackers_in_lists_;
    ++conn_trackers_manager_->num_trackers_ready_for_destruction_;
  } else {
    ++iter_;
  }

  conn_trackers_manager_->CheckConsistency();

  AdvanceToValidTracker();

  return *this;
}

void ConnTrackersManager::TrackersList::TrackersListIterator::AdvanceToValidTracker() {
  while ((iter_ != trackers_->end()) && ((*iter_)->traffic_class().protocol != protocol_)) {
    DCHECK_EQ(protocol_, kProtocolUnknown);
    trackers_->erase(iter_++);
    --conn_trackers_manager_->num_trackers_in_lists_;
    --conn_trackers_manager_->num_tracker_dups_;
  }

  conn_trackers_manager_->CheckConsistency();
}

}  // namespace stirling
}  // namespace pl
