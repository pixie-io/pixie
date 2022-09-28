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

#include <gflags/gflags.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

DEFINE_uint32(num_threads, std::thread::hardware_concurrency() / 2, "Number of worker threads");
DEFINE_uint32(num_threads_sleepy, std::thread::hardware_concurrency() / 2,
              "Number of sleepy worker threads");
DEFINE_uint64(N, 10000000000ULL, "Amount to count per thread");

// These parameters were picked to  result in a thread that does about 30% work on oazizi-lt.
DEFINE_uint64(sleepy_thread_batch_size, 20000000ULL,
              "Amount to count before each sleep (sleepy threads only)");
DEFINE_uint64(sleepy_thread_sleep_ms, 100, "Amount to sleep per iteration");

uint64_t Compute(uint64_t n) {
  uint64_t counter = 0;
  for (uint64_t i = 0; i < n; ++i) {
    counter++;
  }

  return counter;
}

void SleepyCompute(uint64_t n, uint64_t batch_size, uint64_t sleep_ms) {
  for (uint32_t i = 0; i < n / batch_size; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
    Compute(batch_size);
  }
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  uint32_t num_threads = FLAGS_num_threads;
  uint32_t num_threads_sleepy = FLAGS_num_threads_sleepy;

  std::cout << "Number of threads (regular, sleepy): " << num_threads << ", " << num_threads_sleepy
            << std::endl;

  std::cout << "Press ENTER to begin execution" << std::endl;
  std::cin.ignore();

  std::vector<std::thread> threads;

  // Spawn off aggressive threads
  for (uint32_t i = 0; i < num_threads; ++i) {
    threads.push_back(std::thread(&Compute, FLAGS_N));
  }

  // Spawn off intermittent sleepy threads
  for (uint32_t i = 0; i < num_threads; ++i) {
    threads.push_back(std::thread(&SleepyCompute, FLAGS_N, FLAGS_sleepy_thread_batch_size,
                                  FLAGS_sleepy_thread_sleep_ms));
  }

  for (auto& t : threads) {
    t.join();
  }

  return 0;
}
