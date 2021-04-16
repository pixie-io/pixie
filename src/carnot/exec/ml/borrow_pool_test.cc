#include "src/carnot/exec/ml/borrow_pool.h"
#include <gtest/gtest.h>
#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace px {
namespace carnot {
namespace exec {
namespace ml {

TEST(BorrowPool, basic) {
  class A : public std::string {};
  BorrowPool<A> pool;
  pool.Add(std::make_unique<A>());
  pool.Add(std::make_unique<A>());

  {
    auto ptr1 = pool.Borrow();
    EXPECT_NE(ptr1, nullptr);
    EXPECT_EQ(1, pool.Size());
  }
  EXPECT_EQ(2, pool.Size());
  auto ptr1 = pool.Borrow();
  EXPECT_NE(ptr1, nullptr);
  auto ptr2 = pool.Borrow();
  EXPECT_NE(ptr2, nullptr);
  auto ptr3 = pool.Borrow();
  EXPECT_EQ(ptr3, nullptr);
  ptr2.reset();
  ptr3 = pool.Borrow();
  EXPECT_NE(ptr3, nullptr);
}

// Test to check for data races with ASAN/TSAN
TEST(BorrowPool, threaded) {
  BorrowPool<int> pool;
  pool.Add(BorrowPool<int>::StoredPtrType(new int(1)));
  pool.Add(BorrowPool<int>::StoredPtrType(new int(2)));

  std::atomic<int> sum = 0;
  std::atomic<int> null_ptr_cnt = 0;
  std::thread thread_a([&sum, &null_ptr_cnt, &pool] {
    auto ptr1 = pool.Borrow();
    if (ptr1 != nullptr) {
      sum += *ptr1;
    } else {
      null_ptr_cnt += 1;
    }
    auto ptr2 = pool.Borrow();
    if (ptr2 != nullptr) {
      sum += *ptr2;
    } else {
      null_ptr_cnt += 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ptr1.reset();
    ptr2.reset();
  });
  std::thread thread_b([&sum, &null_ptr_cnt, &pool] {
    auto ptr1 = pool.Borrow();
    if (ptr1 != nullptr) {
      sum += *ptr1;
    } else {
      null_ptr_cnt += 1;
    }
    auto ptr2 = pool.Borrow();
    if (ptr2 != nullptr) {
      sum += *ptr2;
    } else {
      null_ptr_cnt += 1;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ptr1.reset();
    ptr2.reset();
  });
  thread_a.join();
  thread_b.join();
  EXPECT_EQ(3, sum);
  EXPECT_EQ(2, null_ptr_cnt);
  EXPECT_EQ(2, pool.Size());
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
