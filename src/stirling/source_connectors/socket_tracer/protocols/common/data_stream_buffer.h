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

#include <gflags/gflags.h>

#include <map>
#include <memory>
#include <string>

#include "src/common/base/base.h"

DECLARE_bool(stirling_data_stream_buffer_always_contiguous_buffer);

namespace px {
namespace stirling {
namespace protocols {

// TODO(james): switch back to concrete DataStreamBuffer (or standard abstract class) once we've
// settled on a DataStreamBuffer implementation.
class DataStreamBufferImpl {
 public:
  virtual ~DataStreamBufferImpl() = default;
  virtual void Add(size_t pos, std::string_view data, uint64_t timestamp) = 0;
  virtual std::string_view Head() = 0;
  virtual StatusOr<uint64_t> GetTimestamp(size_t pos) const = 0;
  virtual void RemovePrefix(ssize_t n) = 0;
  virtual void Trim() = 0;
  virtual size_t size() const = 0;
  virtual size_t capacity() const = 0;
  virtual bool empty() const = 0;
  virtual size_t position() const = 0;
  virtual std::string DebugInfo() const = 0;
  virtual void Reset() = 0;
  virtual void ShrinkToFit() = 0;
};

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
  DataStreamBuffer(size_t max_capacity, size_t max_gap_size, size_t allow_before_gap_size);

  /**
   * Adds data to the buffer at the specified logical position.
   * If inserting the data requires the buffer to expand beyond its capacity,
   * old data will be dropped as the rolling buffer is expanded.
   *
   * @param pos Position at which to insert the data.
   * @param data The data to insert.
   * @param timestamp Timestamp to associate with the data.
   */
  void Add(size_t pos, std::string_view data, uint64_t timestamp) {
    impl_->Add(pos, data, timestamp);
  }

  /**
   * Get all the contiguous data at the head of the buffer.
   * @return A string_view to the data.
   */
  std::string_view Head() { return impl_->Head(); }

  /**
   * Get timestamp recorded for the data at the specified position.
   * If less than previous timestamp, timestamp will be adjusted to be monotonically increasing.
   * @param pos The logical position of the data.
   * @return The timestamp or error if the position does not contain valid data.
   */
  StatusOr<uint64_t> GetTimestamp(size_t pos) { return impl_->GetTimestamp(pos); }

  /**
   * Remove n bytes from the head of the buffer.
   *
   * Negative values for pos are invalid and will not remove anything.
   * In debug mode, negative values will cause a failure.
   */
  void RemovePrefix(ssize_t n) { return impl_->RemovePrefix(n); }

  /**
   * If the head of the buffer contains any non-valid data (never populated),
   * then remove it until reaching the first data added.
   */
  void Trim() { impl_->Trim(); }

  /**
   * Current size of the internal buffer. Not all bytes may be populated.
   */
  size_t size() const { return impl_->size(); }

  /**
   * Current allocated space of the internal buffer.
   */
  size_t capacity() const { return impl_->capacity(); }

  /**
   * Return true if the buffer is empty.
   */
  bool empty() const { return impl_->empty(); }

  /**
   * Logical position of the head of the buffer.
   */
  size_t position() const { return impl_->position(); }

  std::string DebugInfo() const { return impl_->DebugInfo(); }

  /**
   * Resets the entire buffer to an empty state.
   * Intended for hard recovery conditions.
   */
  void Reset() { impl_->Reset(); }

  /**
   * Shrink the internal buffer, so that the allocated memory matches its size.
   * Note this has to be an external API, because `RemovePrefix` is called in situations where it
   * doesn't make sense to shrink.
   */
  void ShrinkToFit() { impl_->ShrinkToFit(); }

 private:
  std::unique_ptr<DataStreamBufferImpl> impl_;
};

}  // namespace protocols
}  // namespace stirling
}  // namespace px
