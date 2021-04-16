#include "src/common/event/api_impl.h"
#include "src/common/event/libuv.h"

namespace px {
namespace event {

DispatcherUPtr event::APIImpl::AllocateDispatcher(std::string_view name) {
  return std::make_unique<LibuvDispatcher>(name, *this, time_system_);
}

const event::TimeSource& APIImpl::TimeSourceRef() const { return *time_system_; }

}  // namespace event
}  // namespace px
