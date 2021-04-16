#pragma once

#include <memory>

#include "src/common/event/api.h"

namespace px {
namespace event {

/**
 * APIImpl is the default implementation of the API.
 */
class APIImpl : public API {
 public:
  explicit APIImpl(TimeSystem* time_system) : time_system_(time_system) {}

  DispatcherUPtr AllocateDispatcher(std::string_view name) override;
  const event::TimeSource& TimeSourceRef() const override;

 private:
  // Unowned pointer to the time_system.
  TimeSystem* time_system_;
};

}  // namespace event
}  // namespace px
