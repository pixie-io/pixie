#pragma once

// TODO(yzhao): According to https://github.com/cilium/cilium/blob/master/Documentation/bpf.rst
// and https://lwn.net/Articles/741773/, kernel 4.16 & llvm 6.0 or newer are required to support BPF
// to BPF calls for C code. Figure out how to detect kernel and llvm versions.
#ifndef __inline
#ifdef SUPPORT_BPF2BPF_CALL
#define __inline
#else
// TODO(yzhao): Clarify the effect on GCC and Clang, and see if we can remove the inline keyword.
#define __inline inline __attribute__((__always_inline__))
#endif  // SUPPORT_BPF2BPF_CALL
#endif  // __inline

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
