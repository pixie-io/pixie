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

#include <signal.h>
#include <stdint.h>

#include <iostream>

uint64_t fib(const uint64_t n) {
  if (n == 0) {
    return 0;
  }
  if (n == 1) {
    return 1;
  }

  const int64_t numIters = n - 2;
  uint64_t prev = 1;
  uint64_t curr = 1;
  uint64_t next = 1;

  for (int64_t i = 0; i < numIters; ++i) {
    next = prev + curr;
    prev = curr;
    curr = next;
  }
  return next;
}

uint64_t fib52() { return fib(52); }

uint64_t fib27() { return fib(27); }

bool run = true;

void SignalHandler(int /*s*/) { run = false; }

int main() {
  signal(SIGINT, SignalHandler);

  uint64_t f52 = 0;
  uint64_t f27 = 0;

  while (run) {
    // Calling fib(N) performs N-2 iterations.
    // fib52() & fib27() are "named" so that their
    // symbols are distinct in the stack traces.
    // The stack trace histo should be approx.:
    // fib27(): 33% of observations
    // fib52(): 66% of observations
    f52 = fib52();
    f27 = fib27();
  }

  std::cout << std::endl;
  std::cout << "F27: " << f27 << std::endl;
  std::cout << "F52: " << f52 << std::endl;
  return 0;
}
