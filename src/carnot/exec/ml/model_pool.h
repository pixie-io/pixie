#pragma once

#include <absl/base/internal/spinlock.h>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>
#include <utility>

#include "src/carnot/exec/ml/borrow_pool.h"
#include "src/carnot/exec/ml/model_executor.h"

namespace pl {
namespace carnot {
namespace exec {
namespace ml {

class ModelPool {
 public:
  using PoolType = BorrowPool<ModelExecutor>;
  using PtrType = PoolType::BorrowedPtrType;

  static std::unique_ptr<ModelPool> Create() { return std::make_unique<ModelPool>(); }

  template <typename TExecutor, typename... Args>
  void CreatePool(Args... args) {
    // TODO(james, PP-2594): currently if you ask for the same type of model with different args the
    // pool will return the first args asked for.
    auto pool = std::make_unique<PoolType>();
    pool->Add(std::make_unique<TExecutor>(args...));
    pool_map_[TExecutor::Type()] = std::move(pool);
  }

  template <typename TExecutor>
  struct DerivedDeleter {
    void operator()(TExecutor* ptr) { deleter_(ptr); }
    PoolType::ReclaimDeleter deleter_;
  };

  template <typename TExecutor, typename... Args>
  std::unique_ptr<TExecutor, DerivedDeleter<TExecutor>> GetModelExecutor(Args... args) {
    if (pool_map_.find(TExecutor::Type()) == pool_map_.end()) {
      CreatePool<TExecutor>(args...);
    }
    auto ptr = pool_map_[TExecutor::Type()]->Borrow();
    while (ptr == nullptr) {
      ptr = pool_map_[TExecutor::Type()]->Borrow();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::unique_ptr<TExecutor, DerivedDeleter<TExecutor>>(
        static_cast<TExecutor*>(ptr.release()), DerivedDeleter<TExecutor>{ptr.get_deleter()});
  }

  std::unordered_map<ModelType, std::unique_ptr<PoolType>> pool_map_;
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace pl
