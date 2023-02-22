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

class ProfilerTest {

  public static long leaf1x() {
    long n = 10000;
    long s = 0;
    long m = 10;

    for(long i=0; i < n; i++ ) {
      long k = i % 2 == 0 ? 7 : 11;
      for(long j=m; j <= n; j += m ) {
        if( j % m == 0 ) {
          s += j;
        }
        if( k % j == 0 ) {
          System.out.println(j);
          System.out.println(k);
          return j;
        }
      }
    }
    return s;
  }

  public static long leaf2x() {
    long m = 5;

    long n = 10000;
    long s = 0;

    for(long i=0; i < n; i++ ) {
      long k = i % 2 == 0 ? 7 : 11;
      for(long j=m; j <= n; j += m ) {
        if( j % m == 0 ) {
          s += j;
        }
        if( k % j == 0 ) {
          System.out.println(j);
          System.out.println(k);
          return j;
        }
      }
    }
    return s;
  }

  public static void main(String[] args) {
    long ntrials = 500000000;
    long update_interval = ntrials / 100000;
    long leaf1xsum = 0;
    long leaf2xsum = 0;

    for(long i=0; i < ntrials; i++) {
      for(long j=0; j < ntrials; j++) {
        // Contrived to do the following:
        // 1. Run for a (really) long time (i.e. until the process is externally killed).
        // 2. Spend twice as much time in leaf2x() vs. leaf1x().
        leaf1xsum = leaf1x();
        leaf2xsum = leaf2x();
        if(j % update_interval == 0) {
          String msg = "Completed %6d trials, leaf1xsum: %d, leaf2xsum: %d.";
          System.out.println(String.format(msg, j, leaf1xsum, leaf2xsum));
        }
      }
    }

    System.out.println(leaf1xsum);
    System.out.println(leaf2xsum);
  }
}
