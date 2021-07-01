/*
 * Copyright 2018- The Pixie Authors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#pragma once

#define __inline inline __attribute__((__always_inline__))

// This macro is essentially a min() function that caps a number.
// It performs the min in a way that keeps the the BPF verifier happy.
// It is essentially a traditional min(), plus a mask that helps old versions of the BPF verifier
// reason about the maximum value of a number.
//
// NOTE: cap must be a power-of-2.
// This is not checked for the caller, and behavior is undefined when this is not true.
//
// Note that we still apply a min() function before masking, otherwise, the mask may create a number
// lower than the min if the original number is greater than the cap_mask.
//
// Example:
//   cap = 16
//   cap-1 = 16-1 = 0xf
//   x = 36 = 0x24
//   BPF_LEN_CAP(x, cap) = 16
//
// However, if we remove the min() before applying the mask, we would get a smaller number.
//   x & (cap-1) = 4
#define BPF_LEN_CAP(x, cap) (x < cap ? (x & (cap - 1)) : cap)

static __inline int32_t read_big_endian_int32(const char* buf) {
  int32_t length;
  bpf_probe_read(&length, 4, buf);
  return bpf_ntohl(length);
}

static __inline int32_t read_big_endian_int16(const char* buf) {
  int16_t val;
  bpf_probe_read(&val, 2, buf);
  return bpf_ntohl(val);
}

// Returns 0 if lhs and rhs compares equal up to n bytes. Otherwise a non-zero value is returned.
// NOTE #1: Cannot use C standard library's strncmp() because that cannot be compiled by BCC.
// NOTE #2: Different from the C standard library's strncmp(), this does not distinguish order.
// NOTE #3: n must be a literal so that the BCC runtime can unroll the inner loop.
// NOTE #4: Loop unrolling increases instruction code, be aware when BPF verifier complains about
// breaching instruction count limit.
static __inline int bpf_strncmp(const char* lhs, const char* rhs, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (lhs[i] != rhs[i]) {
      return 1;
    }
  }
  return 0;
}
