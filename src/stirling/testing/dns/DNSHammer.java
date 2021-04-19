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

import java.io.*;
import java.net.*;
import java.util.Random;

// Hammers the host DNS server with different sites.
// Hammer is used loosely here, since it actually does this every 3 seconds.
// Intended for use in tests, where we want to trace the DNS queries.
// Intentionally chose Java to demonstrate that it uses libc's getaddrinfo() function.

class DNSHammer
{
  public static void main(String args[]) throws InterruptedException
  {
    while (true) {
      try {
        String site = RandomSite();
        InetAddress address = InetAddress.getByName(RandomSite());
        System.out.println(site + ": " + address.getHostAddress());
      } catch (Exception e) {
        System.out.println(e);
      }
      Thread.sleep(3000);
    }
  }

  public static String RandomSite() {
    String alphabet = "abcdefghijklmnopqrstuvwxyz";

    String site = "";
    for (int i = 0; i < 3; i++) {
      site += alphabet.charAt(random.nextInt(alphabet.length()));
    }

    return "www." + site + ".com";
  }

  static Random random = new Random();
}
