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

#pragma once

#include "src/stirling/bpf_tools/bcc_bpf_intf/upid.h"

#define MAX_CMD_SIZE 32

// For reporting process exit. These information is read from task_struct.
struct proc_exit_event_t {
  // The time when this was captured in the BPF time.
  uint64_t timestamp_ns;

  // The unique identifier of the process.
  struct upid_t upid;

  // The exit_code from the task_struct object.
  uint32_t exit_code;

  // The command line of this process.
  char comm[MAX_CMD_SIZE];
};
