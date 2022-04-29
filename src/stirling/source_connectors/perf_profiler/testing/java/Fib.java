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

class JavaFib {
  public static long fib(long n) {
    if (n == 0) {
      return 0;
    }
    if (n == 1) {
      return 1;
    }

    long numIters = n - 2;
    long prev = 1;
    long curr = 1;
    long next = 1;

    for (long i = 0; i < numIters; ++i) {
      next = prev + curr;
      prev = curr;
      curr = next;
    }
    return next;
  }

  public static long fibs1x() {
    // Need to loop here for the stack trace sampler to pick up symbol fibs1x.
    // Newer JVMs can optimize this loop if it computes the same value every time,
    // so, we make it do some "real" work.
    long ntrials = 10000;
    long x = 0;
    for(long i=0; i < ntrials; i++) {
      x += fib(i%53);
    }
    return x;
  }

  public static long fibs2x() {
    // Need to loop here for the stack trace sampler to pick up symbol fibs2x.
    // Newer JVMs can optimize this loop if it computes the same value every time,
    // so, we make it do some "real" work.
    long ntrials = 20000;
    long x = 0;
    for(long i=0; i < ntrials; i++) {
      x += fib(i%53);
    }
    return x;
  }

  public static void main(String[] args) {
    long ntrials = 500000000;
    long update_interval = ntrials / 100000;
    long f1xsum = 0;
    long f2xsum = 0;

    for(long i=0; i < ntrials; i++) {
      for(long j=0; j < ntrials; j++) {
        // Contrived to do the following:
        // 1. Run for a (really) long time (i.e. until the process is externally killed).
        // 2. Spend twice as much time in fibs2x() vs. fibs1x().
        f1xsum = fibs1x();
        f2xsum = fibs2x();
        if(j % update_interval == 0) {
          String msg = "Completed %6d trials, f1xsum: %d, f2xsum: %d.";
          System.out.println(String.format(msg, j, f1xsum, f2xsum));
        }
      }
    }

    System.out.println(f1xsum);
    System.out.println(f2xsum);
  }
}
