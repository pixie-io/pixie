#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include "src/common/event/dispatcher.h"

namespace px {
namespace event {

/**
 * API is the interface for event system.
 */
class API {
 public:
  virtual ~API() = default;

  /**
   * Allocate a new dispatcher.
   * @return unique_ptr to dispatcher.
   */
  virtual DispatcherUPtr AllocateDispatcher(std::string_view name) = 0;

  /**
   * @return a reference to the TimeSource
   */
  virtual const TimeSource& TimeSourceRef() const = 0;
};

using APIUPtr = std::unique_ptr<API>;

}  // namespace event
}  // namespace px
