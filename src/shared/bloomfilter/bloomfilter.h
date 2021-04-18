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

#include <math.h>
#include <memory>
#include <string>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/bloomfilterpb/bloomfilter.pb.h"

namespace px {
namespace bloomfilter {

using XXHash64BloomFilterPB = shared::bloomfilterpb::XXHash64BloomFilter;

class XXHash64BloomFilter {
 public:
  /**
   * Create creates a bloom filter which is sized to meet the criteria for maximum number of
   * entries and the false positive error rate. The false negative error rate is always 0.
   */
  static StatusOr<std::unique_ptr<XXHash64BloomFilter>> Create(int64_t max_entries,
                                                               double error_rate);
  static StatusOr<std::unique_ptr<XXHash64BloomFilter>> FromProto(const XXHash64BloomFilterPB& pb);
  XXHash64BloomFilterPB ToProto();

  /**
   * Insert inserts an item into the bloom filter.
   */
  void Insert(std::string_view item);
  void Insert(const std::string& item) { return Insert(std::string_view(item)); }

  /**
   * Contains checks for the presence of an item in the bloom filter. May return a false positive,
   * but will not return a false negative.
   */
  bool Contains(std::string_view item) const;
  bool Contains(const std::string& item) const { return Contains(std::string_view(item)); }

  /**
   * Get the buffer size in bytes of the bloom filter.
   */
  size_t buffer_size_bytes() const { return buffer_.size(); }

  /**
   * Get the number of hashes used in the bloom filter.
   */
  int num_hashes() const { return num_hashes_; }

 protected:
  XXHash64BloomFilter(int64_t num_bytes, int num_hashes)
      : XXHash64BloomFilter(std::vector<uint8_t>(num_bytes, 0), num_hashes) {}

  XXHash64BloomFilter(const std::vector<uint8_t>& buffer, int32_t num_hashes)
      : num_hashes_(num_hashes), buffer_(buffer) {}

 private:
  void SetBit(int bit_number);
  bool HasBitSet(int bit_number) const;

  const int num_hashes_;
  std::vector<uint8_t> buffer_;
  const uint64_t seed_ = 3091990;
};

}  // namespace bloomfilter
}  // namespace px
