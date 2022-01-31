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

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace protocols {

/**
 * DataStreamBuffer is a buffer for storing traced data events and metadata from BPF.
 * Events are inserted by byte position, and are allowed to arrive out-of-order; in other words,
 * an event at a later position may arrive before an event at an earlier position.
 * It is behaviorally a rolling buffer: Once the maximum capacity is reached, new events that
 * require the buffer to expand into higher positions will cause old data to be dropped if it has
 * not yet been consumed.
 *
 * One way to think of DataStreamBuffer is like a slinky. New events expand the buffer at its tail,
 * while consuming data shrinks the buffer at its head. Unlike the slinky model, however,
 * DataStreamBuffer supports data arriving out-of-order such that they are slotted into the middle
 * of the buffer.
 *
 * The underlying implementation is currently a simple string buffer, but this could be changed
 * in the future, as long as the data is maintained in a contiguous buffer.
 */
class DataStreamBuffer {
 public:
  DataStreamBuffer(size_t max_capacity, size_t max_gap_size, size_t allow_before_gap_size)
      : capacity_(max_capacity),
        max_gap_size_(max_gap_size),
        allow_before_gap_size_(allow_before_gap_size) {}

  /**
   * Adds data to the buffer at the specified logical position.
   * If inserting the data requires the buffer to expand beyond its capacity,
   * old data will be dropped as the rolling buffer is expanded.
   *
   * @param pos Position at which to insert the data.
   * @param data The data to insert.
   * @param timestamp Timestamp to associate with the data.
   */
  void Add(size_t pos, std::string_view data, uint64_t timestamp);

  /**
   * Get all the contiguous data at the specified position of the buffer.
   * @param pos The logical position of the requested data.
   * @return A string_view to the data.
   */
  std::string_view Get(size_t pos) const;

  /**
   * Get all the contiguous data at the head of the buffer.
   * @return A string_view to the data.
   */
  std::string_view Head() const { return Get(position_); }

  /**
   * Get timestamp recorded for the data at the specified position.
   * @param pos The logical position of the data.
   * @return The timestamp or error if the position does not contain valid data.
   */
  StatusOr<uint64_t> GetTimestamp(size_t pos) const;

  /**
   * Remove n bytes from the head of the buffer.
   *
   * Negative values for pos are invalid and will not remove anything.
   * In debug mode, negative values will cause a failure.
   */
  void RemovePrefix(ssize_t n);

  /**
   * If the head of the buffer contains any non-valid data (never populated),
   * then remove it until reaching the first data added.
   */
  void Trim();

  /**
   * Current size of the internal buffer. Not all bytes may be populated.
   */
  size_t size() const { return buffer_.size(); }

  /**
   * Return true if the buffer is empty.
   */
  bool empty() const { return buffer_.empty(); }

  /**
   * Logical position of the head of the buffer.
   */
  size_t position() const { return position_; }

  std::string DebugInfo() const;

  /**
   * Resets the entire buffer to an empty state.
   * Intended for hard recovery conditions.
   */
  void Reset();

  /**
   * Shrink the internal buffer, so that the allocated memory matches its size.
   * Note this has to be an external API, because `RemovePrefix` is called in situations where it
   * doesn't make sense to shrink.
   */
  void ShrinkToFit() { buffer_.shrink_to_fit(); }

 private:
  std::map<size_t, size_t>::const_iterator GetChunkForPos(size_t pos) const;
  void AddNewChunk(size_t pos, size_t size);
  void AddNewTimestamp(size_t pos, uint64_t timestamp);

  void CleanupTimestamps();
  void CleanupChunks();

  // Umbrella that calls CleanupTimestamps and CleanupChunks.
  void CleanupMetadata();

  // Get the end of valid data in the buffer.
  size_t EndPosition();

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
};

}  // namespace protocols
}  // namespace stirling
}  // namespace px
