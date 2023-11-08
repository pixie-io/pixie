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
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "src/common/base/mixins.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"

namespace px {
namespace stirling {
namespace protocols {

/**
 * FixedSizeContiguousBuffer allocates a fixed size (determined at construction) buffer of
 * contiguous bytes. This class provides a string_view-like interface to access the underlying fixed
 * size buffer (also provides a utility method to return an actual string_view into the data). The
 * `RemovePrefix` method just changes the view of the data, invalidating the first n bytes of the
 * buffer without actually resizing the buffer.
 */
class FixedSizeContiguousBuffer : public NotCopyable {
 public:
  ~FixedSizeContiguousBuffer();
  // Passing capacity == 0 is undefined behaviour.
  explicit FixedSizeContiguousBuffer(size_t capacity);
  std::string_view StringView();
  // Invalidate first n bytes of data
  void RemovePrefix(size_t n);
  uint8_t* Data();
  size_t Size() const;
  size_t Capacity() const;

 private:
  uint8_t* data_;
  size_t capacity_;
  size_t offset_ = 0;
};

/**
 * This version of the DataStreamBuffer creates contiguous regions lazily, only when requested by
 * Head().
 */
class LazyContiguousDataStreamBufferImpl : public DataStreamBufferImpl {
 public:
  // Support the same constructor signature as the AlwaysContiguousDataStreamBufferImpl.
  LazyContiguousDataStreamBufferImpl(size_t max_capacity, size_t, size_t)
      : capacity_(max_capacity) {}
  explicit LazyContiguousDataStreamBufferImpl(size_t max_capacity)
      : LazyContiguousDataStreamBufferImpl(max_capacity, 0, 0) {}

  void Add(size_t pos, std::string_view data, uint64_t timestamp) override;

  std::string_view Head() override;

  StatusOr<uint64_t> GetTimestamp(size_t pos) const override;

  void RemovePrefix(ssize_t n) override;

  size_t size() const override;
  size_t capacity() const override;

  bool empty() const override;

  size_t position() const override;

  std::string DebugInfo() const override;

  void Reset() override;

  void Trim() override {}

  void ShrinkToFit() override;

 private:
  // Store individual events separately before lazyily merging them into a contiguous buffer when
  // requested by `Head()`.
  struct Event {
    uint64_t timestamp;
    std::string data;

    // Only allow moving events.
    Event(Event&&) = default;
    Event& operator=(Event&&) = default;
    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
  };

  // Attempt to evict n_bytes worth of data, return the number of bytes evicted.
  size_t EvictBytes(size_t n_bytes);

  // Return whether the events in the `head_` and the first events in `events_` are contiguous. Note
  // that if `head_` is empty and `events_` is non-empty this will return true.
  bool IsHeadAndEventsMergeable() const;

  // Create a new `head_` buffer that contains all the events currently in `head_` in addition to
  // any events in `events_` that are contiguous with the current events in `head_`. This method
  // also updates `head_pos_to_ts_` with the timestamps of the merged events.
  void MergeContiguousEventsIntoHead();

  // Get the byte position of the first event in `events_`.
  size_t FirstEventPos() const;

  // Cleanup `head_pos_to_ts_` by removing all entries before `head_position_ + removed`.
  void CleanupHeadTimestamps(size_t removed);

  const size_t capacity_;

  size_t head_position_ = 0;
  std::unique_ptr<FixedSizeContiguousBuffer> head_;
  std::map<size_t, uint64_t> head_pos_to_ts_;
  uint64_t prev_timestamp_ = 0;

  std::map<size_t, Event> events_;
  size_t events_size_ = 0;
};

}  // namespace protocols
}  // namespace stirling
}  // namespace px
