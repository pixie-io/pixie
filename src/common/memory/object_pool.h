#pragma once
#include <memory>
#include <vector>

#include <absl/base/internal/spinlock.h>
#include "src/common/base/base.h"

namespace pl {
/**
 * This class hangs onto pointer and will deallocate them upon destruction.
 *
 * Concepts and some code similar to Impala.
 */
class ObjectPool final : public pl::NotCopyable {
 public:
  ObjectPool() = default;
  ~ObjectPool() { Clear(); }
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

  absl::base_internal::SpinLock lock_;
  std::vector<Entity> obj_list_;
};

}  // namespace pl
