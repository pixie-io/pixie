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

#include "src/stirling/bpf_tools/bcc_bpf/task_struct_utils.h"
#include "src/stirling/bpf_tools/bcc_bpf/utils.h"

BPF_ARRAY(cgroup_id_output, uint64_t, 1);

int probe_bpf_get_current_cgroup_id(struct pt_regs* ctx) {
  uint64_t cgroup_id = pl_bpf_get_current_cgroup_id();
  int kZero = 0;
  cgroup_id_ouptut.update(&kZero, &cgroup_id);
  return 0;
}
