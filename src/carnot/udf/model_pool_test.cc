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

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "src/carnot/udf/model_pool.h"

DEFINE_string(embedding_dir, "", "Path to embedding.proto");

namespace px {
namespace carnot {
namespace udf {

class TestTransformExecutor : public udf::ModelExecutor {
 public:
  explicit TestTransformExecutor(const std::string&) {}
  static constexpr udf::ModelType Type() { return udf::kTransformer; }
};

TEST(ModelPool, basic) {
  auto p = udf::ModelPool::Create();
  auto executor = p->GetModelExecutor<TestTransformExecutor>(FLAGS_embedding_dir);
  EXPECT_EQ(kTransformer, executor->Type());
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
