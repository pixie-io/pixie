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

#pragma once

#define __inline inline __attribute__((__always_inline__))

// Enable BPF_DEBUG via BUILD.bazel.
//
// Example:
// pl_bpf_cc_resource(
//    name = "socket_trace",
//    src = "socket_trace.c",
//    hdrs = socket_trace_hdrs,
//    defines = ["BPF_DEBUG"],
//    syshdrs = "//src/stirling/bpf_tools/bcc_bpf/system-headers",
//)
//
// Note: Do not try to pass BPF_DEBUG through BCCWrapper's cflags. It won't work
// because the BPF code preprocessor will have already processed the ifdefs during
// the bazel build.

#ifdef BPF_DEBUG
#define bpf_debug_printk(fmt, ...) \
  { bpf_trace_printk(fmt, ##__VA_ARGS__); }
#else
#define bpf_debug_printk(fmt, ...) \
  {}
#endif

// _VAR suffix indicates that the count of bytes being read is equal to the variable's byte size.
#define BPF_PROBE_READ_VAR(value, ptr) bpf_probe_read(&value, sizeof(value), ptr)
#define BPF_PROBE_READ_KERNEL_VAR(value, ptr) bpf_probe_read_kernel(&value, sizeof(value), ptr)

static __inline int32_t read_big_endian_int32(const char* buf) {
  int32_t length;
  BPF_PROBE_READ_VAR(length, buf);
  return bpf_ntohl(length);
}

static __inline int16_t read_big_endian_int16(const char* buf) {
  int16_t val;
  BPF_PROBE_READ_VAR(val, buf);
  return bpf_ntohs(val);
}

// Returns 0 if lhs and rhs compares equal up to n bytes. Otherwise a non-zero value is returned.
// NOTE #1: Cannot use C standard library's strncmp() because that cannot be compiled by BCC.
// NOTE #2: Different from the C standard library's strncmp(), this does not distinguish order.
// NOTE #3: n must be a literal so that the BCC runtime can unroll the inner loop.
// NOTE #4: Loop unrolling increases instruction code, be aware when BPF verifier complains about.
//          breaching instruction count limit.
// NOTE #5: This function is prefixed with px_ since kernels > 5.17 have a builtin version of this
// function.
static __inline int px_bpf_strncmp(const char* lhs, size_t n, const char* rhs) {
  for (size_t i = 0; i < n; ++i) {
    if (lhs[i] != rhs[i]) {
      return 1;
    }
  }
  return 0;
}

// There is a macro min() defined by a kernel header.
// We prefer being more self-contained, so define this with a different name.
#define DEFINE_MIN_FN_FOR_TYPE(type) \
  static __inline type min_##type(type l, type r) { return l < r ? l : r; }
// Define the function for new types if needed
DEFINE_MIN_FN_FOR_TYPE(uint32_t)
DEFINE_MIN_FN_FOR_TYPE(int64_t)
DEFINE_MIN_FN_FOR_TYPE(uint64_t)
DEFINE_MIN_FN_FOR_TYPE(size_t)
#undef DEFINE_MIN_FN_FOR_TYPE
