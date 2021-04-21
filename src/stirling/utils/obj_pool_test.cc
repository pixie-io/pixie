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

#include <string>

#include "src/common/testing/testing.h"

#include "src/stirling/utils/obj_pool.h"

namespace px {
namespace stirling {

struct TestObject {
  std::string str = "uninitialized";
  int value = -1;
};

class ObjPoolTest : public ::testing::Test {
 protected:
  ObjPoolTest() : obj_pool(4) {}
  ObjPool<TestObject> obj_pool;
};

TEST_F(ObjPoolTest, ObjRecycled) {
  std::unique_ptr<TestObject> obj;

  obj = obj_pool.Pop();
  obj->str = "something";
  obj->value = 42;
  TestObject* ptr1 = obj.get();
  obj_pool.Recycle(std::move(obj));

  obj = obj_pool.Pop();
  // Expect the pointer to get recycled.
  EXPECT_EQ(obj.get(), ptr1);
  // The object should be initialized fresh.
  EXPECT_EQ(obj->str, "uninitialized");
  EXPECT_EQ(obj->value, -1);
}

TEST_F(ObjPoolTest, Capacity) {
  std::unique_ptr<TestObject> obj;

  std::vector<std::unique_ptr<TestObject>> uptrs;
  std::vector<TestObject*> ptrs;

  // Pop a bunch of objects.
  for (int i = 0; i < 5; ++i) {
    uptrs.push_back(obj_pool.Pop());
    ptrs.push_back(uptrs.back().get());
  }

  // Now recycle them.
  for (auto& uptr : uptrs) {
    obj_pool.Recycle(std::move(uptr));
  }
  uptrs.clear();

  // Get a bunch of objects again.
  // The first four should be recycled, and the last should be new.
  for (int i = 0; i < 5; ++i) {
    uptrs.push_back(obj_pool.Pop());
  }

  // First four are recycled.
  EXPECT_EQ(uptrs[0].get(), ptrs[3]);
  EXPECT_EQ(uptrs[1].get(), ptrs[2]);
  EXPECT_EQ(uptrs[2].get(), ptrs[1]);
  EXPECT_EQ(uptrs[3].get(), ptrs[0]);

  // The fifth object had to be created fresh, so it should be a new pointer.
  EXPECT_NE(uptrs[4].get(), ptrs[0]);
  EXPECT_NE(uptrs[4].get(), ptrs[1]);
  EXPECT_NE(uptrs[4].get(), ptrs[2]);
  EXPECT_NE(uptrs[4].get(), ptrs[3]);
}

}  // namespace stirling
}  // namespace px
