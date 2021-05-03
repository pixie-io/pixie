/*
 * Copyright © 2018- Pixie Labs Inc.
 * Copyright © 2020- New Relic, Inc.
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of New Relic Inc. and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Pixie Labs Inc. and its suppliers and
 * may be covered by U.S. and Foreign Patents, patents in process,
 * and are protected by trade secret or copyright law. Dissemination
 * of this information or reproduction of this material is strictly
 * forbidden unless prior written permission is obtained from
 * New Relic, Inc.
 *
 * SPDX-License-Identifier: Proprietary
 */

// To compile this utility: gcc -o stirling_ctrl stirling_ctrl.c

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char** argv) {
  if (argc != 4) {
    printf("This utility sends a signal to Stirling (or the PEM) to control its behavior.");
    printf("An opcode specifies the behavior to change:\n");
    printf("  opcode 1: Set global debug level.\n");
    printf("    Argument: debug level\n");
    printf("  opcode 2: Control tracing of individual PIDs\n");
    printf("    Argument: PID\n");
    printf("    Positive PID values enable tracing; negative PID values disable tracing.\n");
    printf("    Note: Can be used multiple times to enable/disable tracing of multiple PIDs.\n");
    printf("\n");
    printf("Remember to use sudo if required.");
    printf("\n");
    printf("Usage: %s <Stirling/PEM PID> <opcode> <value>\n", argv[0]);
    exit(1);
  }

  pid_t stirling_pid = atoi(argv[1]);
  int opcode = atoi(argv[2]);
  int value = atoi(argv[3]);

  // Send the opcode.
  union sigval sigval_opcode = {.sival_int = opcode};
  sigqueue(stirling_pid, SIGUSR2, sigval_opcode);
  usleep(100000);

  // Send the argument.
  union sigval sigval_value = {.sival_int = value};
  sigqueue(stirling_pid, SIGUSR2, sigval_value);
}
