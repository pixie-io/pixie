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
#include <sys/syscall.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

/** This a simple multi-threader compute intensive program.
 * The program simply uses the specified number of threads to count
 * as much as possible for the specified duration.
 *
 * The total amount counted by all threads is then reported as a measure of work done.
 *
 * This tool was used for measuring overheads of BPF programs.
 */

DEFINE_uint32(num_threads, std::thread::hardware_concurrency(), "Number of worker threads");
DEFINE_uint32(num_seconds, 10, "Duration to run, in seconds");

std::atomic<bool> flag_start = false;
std::atomic<bool> flag_exit = false;
std::atomic<uint32_t> thread_registered_count = 0;

std::unordered_map<std::thread::id, pid_t> id_to_tid;
std::mutex id_to_tid_mutex;

/**
 * @brief Worker thread, counts as much as it can.
 *
 * @param p A promise: the amount counted by this thread.
 */
uint64_t Compute(std::promise<uint64_t>&& p) {
  // Report the TID of this worker thread to global space.
  {
    std::lock_guard<std::mutex> guard(id_to_tid_mutex);
    id_to_tid[std::this_thread::get_id()] = syscall(SYS_gettid);
    thread_registered_count++;
  }

  // Wait for global start signal.
  while (!flag_start) {
  }

  // Main loop: keep counting!
  uint64_t counter = 0;
  while (true) {
    counter++;

    // Global stop signal.
    if (flag_exit) {
      break;
    }
  }

  // Report counter value.
  p.set_value(counter);
  return counter;
}

/**
 * @brief Runs the threader_time application, to count as much as possible.
 *
 * @param argv This program uses to positional arguments
 *             num_threads - number of worker threads to spawn
 *             num_seconds - the time alloted to count
 */
int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  uint32_t num_threads = FLAGS_num_threads;
  uint32_t num_seconds = FLAGS_num_seconds;

  std::cout << "Number of threads: " << num_threads << std::endl;
  std::cout << "Duration (seconds): " << num_seconds << std::endl;

  std::vector<std::thread> threads;
  std::vector<std::future<uint64_t>> thread_ret_values;

  flag_exit = false;

  std::cout << "Spawning worker threads" << std::endl;

  // Spawn off aggressive threads
  for (uint32_t i = 0; i < num_threads; ++i) {
    std::promise<uint64_t> p;
    thread_ret_values.push_back(p.get_future());
    threads.push_back(std::thread(&Compute, std::move(p)));
  }

  // Wait for all threads to register
  while (thread_registered_count != num_threads) {
  }

  // Report all TIDs:
  pid_t master_pid = getpid();
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "Master PID: " << master_pid << std::endl;
  std::cout << "Worker TIDs:";
  for (auto& it : id_to_tid) {
    std::cout << " " << it.second;
  }
  std::cout << "\n" << std::endl;

  // std::cout << "Press ENTER to begin execution" << std::endl;
  // std::cin.ignore();
  std::cout << "Starting execution" << std::endl;

  flag_start = true;
  std::this_thread::sleep_for(std::chrono::seconds(num_seconds));

  flag_exit = true;

  for (auto& t : threads) {
    t.join();
  }

  uint64_t total_work = 0;

  for (uint32_t i = 0; i < num_threads; ++i) {
    total_work += thread_ret_values[i].get();
  }

  std::cout << "Total count value: " << total_work << std::endl;

  return 0;
}
