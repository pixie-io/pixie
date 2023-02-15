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

#include <unistd.h>

#include <chrono>
#include <iostream>
#include <thread>

// A test program that create an thread and sleeps for a long time. Used to test the signal
// delivery and tracing code on sched:sched_process_exit tracepoint.

void sleep_fn() {
  std::cout << gettid() << std::endl;
  for (int i = 0; i < 1000; i++) {
    std::cerr << "i=" << i << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

int main(int /*argc*/, char** /*argv*/) {
  std::cout << getpid() << std::endl;
  std::thread sleep_thread(sleep_fn);
  sleep_thread.join();
}
