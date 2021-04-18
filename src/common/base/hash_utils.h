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

namespace px {

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

}  // namespace px
