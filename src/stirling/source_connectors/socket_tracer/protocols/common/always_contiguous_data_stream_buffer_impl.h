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

#include <map>
#include <string>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"

namespace px {
namespace stirling {
namespace protocols {

class AlwaysContiguousDataStreamBufferImpl : public DataStreamBufferImpl {
 public:
  AlwaysContiguousDataStreamBufferImpl(size_t max_capacity, size_t max_gap_size,
                                       size_t allow_before_gap_size)
      : capacity_(max_capacity),
        max_gap_size_(max_gap_size),
        allow_before_gap_size_(allow_before_gap_size) {}

  void Add(size_t pos, std::string_view data, uint64_t timestamp) override;

  std::string_view Head() override { return Get(position_); }

  StatusOr<uint64_t> GetTimestamp(size_t pos) const override;

  void RemovePrefix(ssize_t n) override;

  void Trim() override;

  size_t size() const override { return buffer_.size(); }

  size_t capacity() const override { return buffer_.capacity(); }

  bool empty() const override { return buffer_.empty(); }

  size_t position() const override { return position_; }

  std::string DebugInfo() const override;

  void Reset() override;

  void ShrinkToFit() override { buffer_.shrink_to_fit(); }

 private:
  std::map<size_t, size_t>::const_iterator GetChunkForPos(size_t pos) const;
  void AddNewChunk(size_t pos, size_t size);
  void AddNewTimestamp(size_t pos, uint64_t timestamp);

  // CheckOverlap checks if the chunk to be added as indicated by pos and size
  // overlaps with any existing chunks in the data buffer.
  bool CheckOverlap(size_t pos, size_t size);

  void CleanupTimestamps();
  void CleanupChunks();

  // Umbrella that calls CleanupTimestamps and CleanupChunks.
  void CleanupMetadata();

  // Get the end of valid data in the buffer.
  size_t EndPosition();

  // Get a string_view for the chunk at pos.
  std::string_view Get(size_t pos);

  // Ensure that timestamps are monotonically increasing for a given chunk.
  void EnforceTimestampMonotonicity(size_t chunk_start, size_t chunk_end);

  const size_t capacity_;
  const size_t max_gap_size_;
  const size_t allow_before_gap_size_;

  // Logical position of data stream buffer.
  // In other words, the position of buffer_[0].
  size_t position_ = 0;

  // Buffer where all data is stored.
  // TODO(oazizi): Investigate buffer that is better suited to the rolling buffer (slinky) model.
  std::string buffer_;

  // Map of chunk start positions to chunk sizes.
  // A chunk is a contiguous sequence of bytes.
  // Adjacent chunks are always fused, so a chunk either ends at a gap or the end of the buffer.
  std::map<size_t, size_t> chunks_;

  // Map of positions to timestamps.
  // Unlike chunks_, which will fuse when adjacent, timestamps never fuse.
  // Also, we don't track gaps in the buffer with timestamps; must use chunks_ for that.
  std::map<size_t, uint64_t> timestamps_;
  size_t prev_timestamp_ = 0;
};

}  // namespace protocols
}  // namespace stirling
}  // namespace px
