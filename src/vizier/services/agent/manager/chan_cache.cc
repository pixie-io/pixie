#include "src/vizier/services/agent/manager/chan_cache.h"

#include <vector>

namespace px {
namespace vizier {
namespace agent {

std::shared_ptr<::grpc::Channel> ChanCache::GetChan(std::string_view remote_addr) {
  absl::base_internal::SpinLockHolder lock(&chan_cache_lock_);
  auto it = chan_cache_.find(remote_addr);
  if (it == chan_cache_.end()) {
    return nullptr;
  }
  return it->second.chan;
}

void ChanCache::Add(std::string remote_addr, std::shared_ptr<::grpc::Channel> chan) {
  absl::base_internal::SpinLockHolder lock(&chan_cache_lock_);
  chan_cache_[remote_addr] = {chan, std::chrono::system_clock::now()};
}

Status ChanCache::CleanupChans() {
  absl::base_internal::SpinLockHolder lock(&chan_cache_lock_);
  std::vector<std::string> remote_addrs_to_delete;
  auto time_now = std::chrono::system_clock::now();
  for (const auto& [remote_addr, chan] : chan_cache_) {
    // Get the state of the channel.
    auto state = chan.chan->GetState(/*try_to_connect*/ false);
    if (state == grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN ||
        state == grpc_connectivity_state::GRPC_CHANNEL_TRANSIENT_FAILURE) {
      remote_addrs_to_delete.push_back(remote_addr);
      continue;
    }
    std::chrono::nanoseconds age = time_now - chan.start_time;
    // If the age of the channel is still warming up, we don't kill it for being idle.
    if (age < warm_up_period_) {
      continue;
    }

    if (state == grpc_connectivity_state::GRPC_CHANNEL_IDLE) {
      remote_addrs_to_delete.push_back(remote_addr);
    }
  }

  for (const std::string& remote_addr : remote_addrs_to_delete) {
    chan_cache_.erase(remote_addr);
  }
  return Status::OK();
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
