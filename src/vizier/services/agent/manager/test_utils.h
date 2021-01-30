#pragma once

#include <vector>

#include "src/common/event/dispatcher.h"
#include "src/common/event/nats.h"

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

}  // namespace agent
}  // namespace vizier
}  // namespace pl
