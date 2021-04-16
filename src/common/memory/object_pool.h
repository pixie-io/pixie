#pragma once
#include <memory>
#include <string>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include "src/common/base/base.h"

namespace px {
/**
 * This class hangs onto pointer and will deallocate them upon destruction.
 *
 * Concepts and some code similar to Impala.
 */
class ObjectPool final : public px::NotCopyable {
 public:
  ObjectPool() = default;
  explicit ObjectPool(std::string_view name) : name_(name) {
    VLOG(1) << "Creating Object Pool: " << name_;
  }

  ~ObjectPool() {
    Clear();
    VLOG_IF(1, !name_.empty()) << "Deleting Object Pool: " << name_;
  }
  /**
   * Take ownership of passed in pointer.
   *
   * @tparam T The entity type to track.
   * @param entity A pointer to the entity.
   * @return The pointer to the entity.
   */
  template <typename T>
  T* Add(T* entity) {
    absl::base_internal::SpinLockHolder lock(&lock_);
    obj_list_.emplace_back(Entity{entity, [](void* obj) { delete reinterpret_cast<T*>(obj); }});
    return entity;
  }

  void Clear() {
    absl::base_internal::SpinLockHolder lock(&lock_);
    for (auto& obj : obj_list_) {
      obj.delete_fn(obj.obj);
    }
    obj_list_.clear();
  }

 private:
  // A generic deletion function pointer. Deletes its first argument.
  using DeleteFn = void (*)(void*);

  // For each object, a pointer to the object and a function that deletes it.
  struct Entity {
    void* obj;
    DeleteFn delete_fn;
  };

  const std::string name_;
  absl::base_internal::SpinLock lock_;
  std::vector<Entity> obj_list_;
};

}  // namespace px
