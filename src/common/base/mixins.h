#pragma once

namespace px {
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

/**
 * Inheriting this class will make disallow automatic copies and moves in
 * the subclass.
 */
class NotCopyMoveable : public NotCopyable {
 public:
  NotCopyMoveable(NotCopyMoveable&&) = delete;
  NotCopyMoveable() = default;
};

}  // namespace px
