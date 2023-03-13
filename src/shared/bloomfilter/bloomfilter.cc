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

#include <math.h>
#include <memory>
#include <utility>

#include "src/common/base/base.h"
#include "src/shared/bloomfilter/bloomfilter.h"

PX_SUPPRESS_WARNINGS_START()
// NOLINTNEXTLINE: build/include_subdir
#include "xxhash.h"
PX_SUPPRESS_WARNINGS_END()

namespace px {
namespace bloomfilter {

StatusOr<std::unique_ptr<XXHash64BloomFilter>> XXHash64BloomFilter::Create(int64_t max_entries,
                                                                           double error_rate) {
  if (error_rate <= 0.0 || error_rate >= 1.0) {
    return error::Internal(
        "Bloom filter error rate must be greater than 0 and less than 1, received %e", error_rate);
  }
  if (max_entries <= 0) {
    return error::Internal("Bloom filter must have a maximum of at least 1 entry, received %d",
                           max_entries);
  }

  // From Wikipedia: https://en.wikipedia.org/wiki/Bloom_filter
  // bits per entry = ln(error_rate)/ln(2)^2
  double bpe = -(std::log(error_rate) / std::pow(std::log(2), 2));
  int64_t num_bits = static_cast<int64_t>(std::ceil(max_entries * bpe));
  int64_t num_bytes = (num_bits / 8) + ((num_bits % 8) ? 1 : 0);

  // num hashes = ln(2) * bpe
  int32_t num_hashes = static_cast<int32_t>(std::ceil(std::log(2) * bpe));

  return std::unique_ptr<XXHash64BloomFilter>(new XXHash64BloomFilter(num_bytes, num_hashes));
}

StatusOr<std::unique_ptr<XXHash64BloomFilter>> XXHash64BloomFilter::FromProto(
    const XXHash64BloomFilterPB& pb) {
  auto bytes_str = pb.data();

  if (!bytes_str.size()) {
    return error::Internal("Received 0 bytes in BloomFilter data field");
  }
  if (!pb.num_hashes()) {
    return error::Internal("Received 0 hash functions in BloomFilter num_hashes field");
  }

  std::vector<uint8_t> data{bytes_str.begin(), bytes_str.end()};
  return std::unique_ptr<XXHash64BloomFilter>(new XXHash64BloomFilter(data, pb.num_hashes()));
}

XXHash64BloomFilterPB XXHash64BloomFilter::ToProto() {
  XXHash64BloomFilterPB output;
  output.set_num_hashes(num_hashes_);
  std::string bytes_str{buffer_.begin(), buffer_.end()};
  output.set_data(std::move(bytes_str));
  return output;
}

void XXHash64BloomFilter::SetBit(int bit_number) {
  int byte_index = bit_number >> 3;
  int mask = 1 << (bit_number % 8);
  buffer_[byte_index] = buffer_[byte_index] | mask;
}

bool XXHash64BloomFilter::HasBitSet(int bit_number) const {
  int byte_index = bit_number >> 3;
  int mask = 1 << (bit_number % 8);
  return buffer_[byte_index] & mask;
}

void XXHash64BloomFilter::Insert(std::string_view item) {
  uint64_t a = XXH64(item.data(), item.size(), seed_);
  uint64_t b = XXH64(item.data(), item.size(), a);

  for (auto i = 0; i < num_hashes_; ++i) {
    // Use int128 because the combination of uint64s below (the underyling type of XXH64) could
    // overflow.
    absl::uint128 x = a + i * b;
    int bit_number = static_cast<int>(x % (buffer_.size() << 3));
    SetBit(bit_number);
  }
}

bool XXHash64BloomFilter::Contains(std::string_view item) const {
  uint64_t a = XXH64(item.data(), item.size(), seed_);
  uint64_t b = XXH64(item.data(), item.size(), a);

  for (auto i = 0; i < num_hashes_; ++i) {
    absl::uint128 x = a + i * b;
    int bit_number = static_cast<int>(x % (buffer_.size() << 3));
    if (!HasBitSet(bit_number)) {
      return false;
    }
  }
  return true;
}

}  // namespace bloomfilter
}  // namespace px
