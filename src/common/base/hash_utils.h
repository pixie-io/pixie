#pragma once

namespace pl {

/**
 * Combines two 64-bit hash values (borrowed from CityHash).
 * @param h1 The first hash.
 * @param h2 The second hash.
 * Both hashes can be from the same family.
 * @return A 64-bit combined hash.
 */
inline uint64_t HashCombine(uint64_t h1, uint64_t h2) {
  // Murmur-inspired hashing.
  const uint64_t kMul = 0x9ddfea08eb382d69ULL;
  uint64_t a = (h1 ^ h2) * kMul;
  a ^= (a >> 47);
  uint64_t b = (h2 ^ a) * kMul;
  b ^= (b >> 47);
  b *= kMul;
  return b;
}

}  // namespace pl
