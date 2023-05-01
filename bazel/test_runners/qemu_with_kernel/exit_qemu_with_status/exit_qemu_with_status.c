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
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>
#include <unistd.h>

#define EXIT_PORT 0xf4

int main(int argc, char** argv) {
  int status;
  if (argc != 2) {
    exit(1);
  }
  status = atoi(argv[1]);

  if (ioperm(EXIT_PORT, 8, 1)) {
    perror("ioperm");
    exit(1);
  }

  unsigned char statusb = (unsigned char)status;
  // QEMU transforms this into (status << 1) | 1;
  // We don't want to interfere with qemu error code,
  // so we further make sure the codes are > 128 for exit codes from our tests.
  // If we shift by 6, the additinal shift of 1 by qemu will make the exit code > 128.
  statusb |= (1 << 6);
  outb(statusb, EXIT_PORT);

  // Should never get here.
  printf(
      "Exit failed. Make sure to include '-device isa-debug-exit,iobase=0xf4,iosize=0x4'"
      " in the qemu command?\n");
  exit(1);
}
