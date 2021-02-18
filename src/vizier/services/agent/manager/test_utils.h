#pragma once

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "src/common/event/dispatcher.h"
#include "src/common/event/nats.h"
#include "src/shared/metadata/state_manager.h"

namespace pl {
namespace vizier {
namespace agent {

template <typename TMsg>
class FakeNATSConnector : public event::NATSConnector<TMsg> {
 public:
  FakeNATSConnector() : event::NATSConnector<TMsg>("", "", "", nullptr) {}
  ~FakeNATSConnector() override {}

  Status Connect(event::Dispatcher*) override { return Status::OK(); }

  Status Publish(const TMsg& msg) override {
    published_msgs_.push_back(msg);
    return Status::OK();
  }

  const std::vector<TMsg>& published_msgs() const { return published_msgs_; }

 private:
  std::vector<TMsg> published_msgs_;
};

class FakeAgentMetadataStateManager : public md::AgentMetadataStateManager {
 public:
  FakeAgentMetadataStateManager() : FakeAgentMetadataStateManager(/*metadata filter*/ nullptr) {}

  explicit FakeAgentMetadataStateManager(md::AgentMetadataFilter* metadata_filter)
      : metadata_filter_(metadata_filter) {
    metadata_state_ =
        std::make_shared<pl::md::AgentMetadataState>("myhost", 1, sole::uuid4(), "mypod");
  }

  virtual ~FakeAgentMetadataStateManager() = default;

  md::AgentMetadataFilter* metadata_filter() const override { return metadata_filter_; }

  std::shared_ptr<const md::AgentMetadataState> CurrentAgentMetadataState() override {
    return metadata_state_;
  }

  Status PerformMetadataStateUpdate() override {
    state_updated_count_++;
    return Status::OK();
  }

  Status AddK8sUpdate(std::unique_ptr<md::ResourceUpdate> update) override {
    updates_.push_back(std::move(update));
    return Status::OK();
  }

  int32_t num_k8s_updates() const { return updates_.size(); }
  md::ResourceUpdate* k8s_update(int32_t i) const { return updates_[i].get(); }

  void SetServiceCIDR(CIDRBlock cidr) override { cidr_ = cidr; }

  void SetPodCIDR(std::vector<CIDRBlock> cidrs) override { pod_cidr_ = cidrs; }

  void AddPIDStatusEvent(std::unique_ptr<md::PIDStatusEvent> event) {
    pid_status_events_.push(std::move(event));
  }

  std::unique_ptr<md::PIDStatusEvent> GetNextPIDStatusEvent() override {
    if (!pid_status_events_.size()) {
      return nullptr;
    }
    auto event = std::move(pid_status_events_.front());
    pid_status_events_.pop();
    return event;
  }

 private:
  md::AgentMetadataFilter* metadata_filter_ = nullptr;
  std::shared_ptr<const md::AgentMetadataState> metadata_state_;
  int32_t state_updated_count_ = 0;
  std::vector<std::unique_ptr<md::ResourceUpdate>> updates_;
  CIDRBlock cidr_;
  std::vector<CIDRBlock> pod_cidr_;
  std::queue<std::unique_ptr<md::PIDStatusEvent>> pid_status_events_;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
