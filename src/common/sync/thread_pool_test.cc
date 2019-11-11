#include "src/common/sync/thread_pool.h"

#include <gtest/gtest.h>

#include <future>
#include <queue>
#include <thread>
#include <vector>

namespace pl {
namespace sync {

TEST(ThreadPool, starting_and_terminating_pool_should_not_hang) {
  ThreadPool pool(2);
  auto res = pool.Enqueue(
      "", [](int i) -> int { return i; }, 1);
  EXPECT_EQ(1, res.ConsumeValueOrDie().get());
}

TEST(ThreadPool, no_args_no_return) {
  bool ran = false;
  {
    ThreadPool pool(2);
    auto res = pool.Enqueue("", [&ran]() {
      ran = true;
      return;
    });
    pool.WaitDrain();
  }
  EXPECT_EQ(true, ran);
}

}  // namespace sync
}  // namespace pl
