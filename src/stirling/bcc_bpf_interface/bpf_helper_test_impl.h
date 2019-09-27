#pragma once

#include <arpa/inet.h>

// This file includes impls of BPF helper functions [1], which mimic their behavior in a userland
// context, instead of BPF.
//
// [1] https://github.com/iovisor/bpf-docs/blob/master/bpf_helpers.rst

static int bpf_probe_read(void* dest, size_t len, const void* src) {
  memcpy(dest, src, len);
  return 0;
}

static uint32_t bpf_ntohl(uint32_t v) { return ntohl(v); }
