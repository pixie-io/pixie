#pragma once
#include <memory>
#include <utility>
#include <vector>

#include <absl/base/internal/spinlock.h>

namespace px {
namespace carnot {
namespace exec {
namespace ml {

template <typename T>
class BorrowPool {
 public:
  using StoredPtrType = std::unique_ptr<T>;
  struct ReclaimDeleter {
    void operator()(T* ptr) {
      if (ptr != nullptr) {
        absl::base_internal::SpinLockHolder l(pool_lock_);
        StoredPtrType stored_ptr(ptr);
        pool_->push_back(std::move(stored_ptr));
      }
    }
    std::vector<StoredPtrType>* pool_;
    absl::base_internal::SpinLock* pool_lock_;
  };
  using BorrowedPtrType = std::unique_ptr<T, ReclaimDeleter>;

  void Add(StoredPtrType ptr) {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    pool_.push_back(std::move(ptr));
  }

  BorrowedPtrType Borrow() {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    if (pool_.size() == 0) {
      return nullptr;
    }
    auto raw_ptr = pool_.back().release();
    pool_.pop_back();
    BorrowedPtrType ptr(raw_ptr, ReclaimDeleter{&pool_, &pool_lock_});
    return ptr;
  }

  size_t Size() {
    absl::base_internal::SpinLockHolder l(&pool_lock_);
    return pool_.size();
  }

 private:
  std::vector<StoredPtrType> pool_ GUARDED_BY(pool_lock_);
  absl::base_internal::SpinLock pool_lock_;
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
