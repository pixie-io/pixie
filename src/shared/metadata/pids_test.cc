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

#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "src/shared/metadata/pids.h"

namespace px {
namespace md {

class PIDInfoTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    info_ = std::make_unique<PIDInfo>(UPID(1, 2, 3), "/usr/bin/python3", "test.py --abc=def",
                                      "container_1");
  }
  std::unique_ptr<PIDInfo> info_;
};

TEST_F(PIDInfoTest, BasicAccessors) {
  EXPECT_EQ(UPID(1, 2, 3), info_->upid());
  EXPECT_EQ(0, info_->stop_time_ns());
  EXPECT_EQ(3, info_->start_time_ns());
  EXPECT_EQ("test.py --abc=def", info_->cmdline());
  EXPECT_EQ("container_1", info_->cid());
}

TEST_F(PIDInfoTest, OperatorEq) {
  EXPECT_EQ(PIDInfo(UPID(1, 2, 3), "/usr/bin/python3", "test.py --abc=def", "container_1"), *info_);

  EXPECT_NE(PIDInfo(UPID(1, 2, 4), "java", "test.py --abc=def", "container_1"), *info_);

  EXPECT_NE(PIDInfo(UPID(1, 2, 4), "/usr/bin/python3", "test.py --abc=def", "container_1"), *info_);

  EXPECT_NE(PIDInfo(UPID(1, 2, 3), "/usr/bin/python3", "est.py --abc=def", "container_1"), *info_);

  EXPECT_NE(PIDInfo(UPID(1, 2, 3), "/usr/bin/python3", "test.py --abc=def", "container_2"), *info_);
}

TEST_F(PIDInfoTest, OperatorEqStopTime) {
  PIDInfo other(UPID(1, 2, 3), "/usr/bin/python3", "test.py --abc=def", "container_1");

  other.set_stop_time_ns(1000);
  EXPECT_NE(other, *info_);

  info_->set_stop_time_ns(1000);
  EXPECT_EQ(other, *info_);
}

TEST_F(PIDInfoTest, Clone) { EXPECT_EQ(*info_, *info_->Clone()); }

TEST(PIDStartedEvent, CheckInfo) {
  PIDInfo pid_info(UPID(1, 2, 3), "", "test.py --abc=def", "container_1");
  PIDStartedEvent ev(pid_info);

  EXPECT_EQ(PIDStatusEventType::kStarted, ev.type);
  EXPECT_EQ(pid_info, ev.pid_info);
}

TEST(PIDTerminatedEvent, CheckUPIDAndStopTime) {
  PIDTerminatedEvent ev(UPID(1, 2, 3), 100);

  EXPECT_EQ(UPID(1, 2, 3), ev.upid);
  EXPECT_EQ(100, ev.stop_time_ns);
  EXPECT_EQ(PIDStatusEventType::kTerminated, ev.type);
}
}  // namespace md
}  // namespace px
