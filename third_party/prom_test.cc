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

#include <prometheus/counter.h>
#include <prometheus/registry.h>

#include <iostream>
#include <memory>

int main() {
  auto registry = std::make_shared<prometheus::Registry>();
  auto& packet_counter = prometheus::BuildCounter()
                             .Name("observed_packets_total")
                             .Help("Number of observed packets")
                             .Register(*registry);
  auto& tcp_rx_counter = packet_counter.Add({{"protocol", "tcp"}, {"direction", "rx"}});

  tcp_rx_counter.Increment();

  if (tcp_rx_counter.Value() != 1) {
    std::cout << "Failed!" << std::endl;
    return 1;
  }
  std::cout << "Works!" << std::endl;
}
