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

#include <gtest/gtest.h>

#include "src/common/testing/testing.h"
#include "src/vizier/services/agent/pem/pem_manager.h"

DECLARE_bool(disable_SSL);

namespace px {
namespace vizier {
namespace agent {

class PEMManagerTest : public ::testing::Test {
 protected:
  PEMManagerTest() {
    FLAGS_disable_SSL = true;
    agent_info_ = agent::Info{};
    agent_info_.agent_id = sole::uuid4();
    agent_info_.hostname = "hostname";
    agent_info_.pod_name = "pod_name";
    agent_info_.host_ip = "host_ip";
    agent_info_.kernel_version =
        system::ParseKernelVersionString("5.15.0-106-generic").ValueOrDie();
  }
  agent::Info agent_info_;
};

TEST_F(PEMManagerTest, Constructor) {
  auto manager = PEMManager::Create(true, agent_info_.agent_id, agent_info_.pod_name,
                                    agent_info_.host_ip, "nats_url", agent_info_.kernel_version)
                     .ConsumeValueOrDie();
  EXPECT_EQ(manager->info()->agent_id, agent_info_.agent_id);
  EXPECT_EQ(manager->info()->pod_name, agent_info_.pod_name);
  EXPECT_EQ(manager->info()->host_ip, agent_info_.host_ip);
  EXPECT_EQ(manager->info()->kernel_version, agent_info_.kernel_version);
}

}  // namespace agent
}  // namespace vizier
}  // namespace px
