#pragma once

namespace pl {
/**
 * Inheriting this class will make disallow automatic copies in
 * the subclass.
 */
class NotCopyable {
 public:
  NotCopyable(NotCopyable const&) = delete;
  NotCopyable& operator=(NotCopyable const&) = delete;
  NotCopyable() = default;
};

}  // namespace pl
