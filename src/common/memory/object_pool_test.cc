/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/memory/object_pool.h"
#include <gtest/gtest.h>

namespace px {

class TestObject {
 public:
  TestObject() = delete;
  explicit TestObject(int* destroy_count) : destroy_count_(destroy_count) {}
  ~TestObject() { (*destroy_count_)++; }

 private:
  int* destroy_count_;
};

class TestObjectTwo {
 public:
  TestObjectTwo() = delete;
  explicit TestObjectTwo(int* destroy_count) : destroy_count_(destroy_count) {}
  ~TestObjectTwo() { (*destroy_count_)++; }

 private:
  int* destroy_count_;
};

TEST(object_pool_test, test_destroy) {
  int count = 0;
  {
    ObjectPool pool;
    pool.Add(new TestObject(&count));
    pool.Add(new TestObject(&count));
    pool.Add(new TestObject(&count));
    EXPECT_EQ(0, count);
  }
  EXPECT_EQ(3, count);
}

TEST(object_pool_test, test_clear) {
  int count = 0;
  ObjectPool pool;
  pool.Add(new TestObject(&count));
  pool.Add(new TestObject(&count));
  pool.Add(new TestObject(&count));
  EXPECT_EQ(0, count);
  pool.Clear();
  EXPECT_EQ(3, count);
}

TEST(object_pool_test, test_different_objects) {
  int count = 0;
  int count2 = 0;
  {
    ObjectPool pool;
    pool.Add(new TestObject(&count));
    pool.Add(new TestObjectTwo(&count2));
    pool.Add(new TestObject(&count));
    EXPECT_EQ(0, count);
    EXPECT_EQ(0, count2);
  }
  EXPECT_EQ(2, count);
  EXPECT_EQ(1, count2);
}

}  // namespace px
