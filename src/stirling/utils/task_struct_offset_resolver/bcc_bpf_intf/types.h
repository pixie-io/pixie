#pragma once

struct buf {
  uint64_t u64words[4096 / sizeof(uint64_t)];
};
