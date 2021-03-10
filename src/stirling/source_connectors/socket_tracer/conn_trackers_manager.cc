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

void ConnTrackersManager::UpdateProtocol(pl::stirling::ConnTracker* tracker,
                                         std::optional<TrafficProtocol> old_protocol) {
  // If the tracker is ReadyForDestruction(), then it should not be a member of any protocol list.
  if (tracker->ReadyForDestruction()) {
    // Since it is not part of any protocol list, it should not have a back pointer to one.
    DCHECK(!tracker->back_pointer_.has_value());
    return;
  }

  if (old_protocol.has_value()) {
    // If an old protocol is specified, then the tracker should also have been set up
    // with a back pointer to its list.
    DCHECK(tracker->back_pointer_.has_value());

    if (old_protocol.value() == tracker->traffic_class().protocol) {
      // Didn't really move, so nothing to update.
      return;
    }

    // Remove tracker from previous list.
    conn_trackers_by_protocol_[old_protocol.value()].erase(tracker->back_pointer_.value());
    --num_trackers_in_lists_;
  } else {
    // If no old protocol is specified, the the tracker should not be in any list.
    // Currently, this should only be possible on initialization of a new tracker.
    DCHECK(!tracker->back_pointer_.has_value());
  }

  // Add tracker to new list based on its current protocol.
  conn_trackers_by_protocol_[tracker->traffic_class().protocol].push_back(tracker);
  tracker->back_pointer_ = --conn_trackers_by_protocol_[tracker->traffic_class().protocol].end();
  ++num_trackers_in_lists_;

  DebugChecks();
}

ConnTracker& ConnTrackersManager::GetOrCreateConnTracker(struct conn_id_t conn_id) {
  const uint64_t conn_map_key = GetConnMapKey(conn_id.upid.pid, conn_id.fd);
  DCHECK_NE(conn_map_key, 0) << "Connection map key cannot be 0, pid must be wrong";

  auto& conn_trackers = connection_trackers_[conn_map_key];
  ConnTracker& conn_tracker = conn_trackers[conn_id.tsid];

  // If conn_trackers[conn_id.tsid] does not exist, a new one will be created in the map.
  // New trackers always have TSID == 0, while BPF should never generate a TSID of zero,
  // so we use this as a way of detecting new trackers.
  // TODO(oazizi): Find a more direct way of detecting new trackers.
  const bool new_tracker = (conn_tracker.conn_id().tsid == 0);
  if (new_tracker) {
    ++num_trackers_;

    conn_tracker.manager_ = this;
    UpdateProtocol(&conn_tracker, {});

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

  DebugChecks();
  return conn_tracker;
}

StatusOr<const ConnTracker*> ConnTrackersManager::GetConnTracker(uint32_t pid, uint32_t fd) const {
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

  DebugChecks();
}

Status ConnTrackersManager::TestOnlyCheckConsistency() const {
  // A set used for looking for duplicate trackers.
  std::set<ConnTracker*> trackers_set;

  for (const auto& [protocol, conn_trackers_list] : conn_trackers_by_protocol_) {
    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnTracker* tracker = *iter;

      // Check that tracker exists in connection_trackers_ (i.e. that the pointer is valid).
      // If the pointer is not valid, this will likely cause a crash or ASAN error.
      const uint64_t conn_map_key =
          GetConnMapKey(tracker->conn_id().upid.pid, tracker->conn_id().fd);
      DCHECK_NE(conn_map_key, 0) << "Connection map key cannot be 0, pid must be wrong";
      auto tracker_set_it = connection_trackers_.find(conn_map_key);
      if (tracker_set_it == connection_trackers_.end()) {
        return error::Internal(
            "Tracker $0 in the protocol lists not found in connection_trackers_.",
            ToString(tracker->conn_id()));
      }

      const auto& tracker_generations = tracker_set_it->second;
      auto tracker_it = tracker_generations.find(tracker->conn_id().tsid);
      if (tracker_it == tracker_generations.end()) {
        return error::Internal(
            "Tracker $0 in the protocol lists not found in connection_trackers_.",
            ToString(tracker->conn_id()));
      }

      // Check that the pointer only shows up once across all lists.
      // NOLINTNEXTLINE: whitespace/braces
      auto [unused, inserted] = trackers_set.insert(tracker);
      if (!inserted) {
        return error::Internal("Tracker $0 found in two lists.", ToString(tracker->conn_id()));
      }
    }
  }

  return Status::OK();
}

void ConnTrackersManager::DebugChecks() const {
  DCHECK_EQ(num_trackers_, num_trackers_in_lists_ + num_trackers_ready_for_destruction_);
}

std::string ConnTrackersManager::DebugInfo() const {
  std::string out;

  for (const auto& [protocol, conn_trackers_list] : conn_trackers_by_protocol_) {
    absl::StrAppend(&out,
                    absl::Substitute("protocol=$0 num_trackers=$1", magic_enum::enum_name(protocol),
                                     conn_trackers_list.size()));

    for (auto iter = conn_trackers_list.begin(); iter != conn_trackers_list.end(); ++iter) {
      ConnTracker* tracker = *iter;

      absl::StrAppend(&out,
                      absl::Substitute(
                          "   conn_id=$0 protocol=$1 state=$2 zombie=$3 ready_for_destruction=$4\n",
                          ToString(tracker->conn_id()),
                          magic_enum::enum_name(tracker->traffic_class().protocol),
                          magic_enum::enum_name(tracker->state()), tracker->IsZombie(),
                          tracker->ReadyForDestruction()));
    }
  }

  return out;
}

//-----------------------------------------------------------------------------
// TrackersListIterator
//-----------------------------------------------------------------------------

ConnTrackersManager::TrackersList::TrackersListIterator::TrackersListIterator(
    std::list<ConnTracker*>* trackers, std::list<ConnTracker*>::iterator iter,
    ConnTrackersManager* conn_trackers_manager)
    : trackers_(trackers), iter_(iter), conn_trackers_manager_(conn_trackers_manager) {}

bool ConnTrackersManager::TrackersList::TrackersListIterator::operator!=(
    const TrackersListIterator& other) {
  return other.iter_ != this->iter_;
}

ConnTracker* ConnTrackersManager::TrackersList::TrackersListIterator::operator*() {
  ConnTracker* tracker = *iter_;
  // Since a tracker can only become ready for destruction in a previous iteration via operator++,
  // and because operator++ would remove such trackers,  we don't expect to see any trackers are
  // ReadyForDestruction here.
  DCHECK(!tracker->ReadyForDestruction());
  return tracker;
}

ConnTrackersManager::TrackersList::TrackersListIterator
ConnTrackersManager::TrackersList::TrackersListIterator::operator++() {
  if ((*iter_)->ReadyForDestruction()) {
    (*iter_)->back_pointer_.reset();
    trackers_->erase(iter_++);
    --conn_trackers_manager_->num_trackers_in_lists_;
    ++conn_trackers_manager_->num_trackers_ready_for_destruction_;
  } else {
    ++iter_;
  }

  return *this;
}

}  // namespace stirling
}  // namespace pl
