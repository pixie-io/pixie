/*
 * This code runs using bpf in the Linux kernel.
 * Copyright 2018- The Pixie Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.
// List of stubs of BCC/BPF helper functions.

#pragma once

#ifdef __cplusplus

// These functions are used by CPP test code to test BCC C code. Essentially, these functions are
// stubbed version of BPF helper functions called by the BCC C code that are being tested.
// See src/stirling/source_connectors/socket_tracer/bcc_bpf/protocol_inference_test.cc
// for such an example.

#include "src/common/base/base.h"

inline struct task_struct* bpf_get_current_task() {
  DCHECK(false) << "bpf_get_current_task() is not implemented";
  return NULL;
}

template <typename TDestCharType, typename TSrcCharType>
inline void bpf_probe_read(TDestCharType* destination, size_t len, TSrcCharType* src) {
  memcpy(destination, src, len);
}

inline int32_t bpf_ntohl(int32_t val) { return ntohl(val); }

inline int16_t bpf_ntohs(int16_t val) { return ntohs(val); }

#endif
