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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "src/common/base/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/lazy_contiguous_data_stream_buffer_impl.h"

namespace px {
namespace stirling {
namespace protocols {

FixedSizeContiguousBuffer::FixedSizeContiguousBuffer(size_t capacity)
    : data_(new uint8_t[capacity]), capacity_(capacity) {
  DCHECK_GT(capacity, 0U);
}

FixedSizeContiguousBuffer::~FixedSizeContiguousBuffer() {
  if (capacity_ == 0) {
    return;
  }
  // TODO(james): investigate sized delete.
  delete[] data_;
}

std::string_view FixedSizeContiguousBuffer::StringView() {
  return {reinterpret_cast<const char*>(Data()), Size()};
}

void FixedSizeContiguousBuffer::RemovePrefix(size_t n) {
  offset_ += n;
  if (offset_ >= capacity_) {
    offset_ = capacity_;
  }
}

uint8_t* FixedSizeContiguousBuffer::Data() {
  DCHECK_GT(capacity_, 0U);
  return data_ + offset_;
}

size_t FixedSizeContiguousBuffer::Size() const { return capacity_ - offset_; }
size_t FixedSizeContiguousBuffer::Capacity() const { return capacity_; }

void LazyContiguousDataStreamBufferImpl::Add(size_t pos, std::string_view data,
                                             uint64_t timestamp) {
  if (data.size() == 0) {
    // Ignore empty events.
    return;
  }
  if (head_ != nullptr && pos < head_position_) {
    // Ignore events that are too old. This is a performance optimization to avoid recreating the
    // head_ buffer.
    return;
  }
  if (data.size() > capacity_) {
    pos += data.size() - capacity_;
    data.remove_prefix(data.size() - capacity_);
  }
  if (size() + data.size() > capacity_) {
    EvictBytes(size() + data.size() - capacity_);
  }

  events_size_ += data.size();
  auto event = Event{timestamp, std::string(data)};
  events_.emplace(pos, std::move(event));
}

size_t LazyContiguousDataStreamBufferImpl::EvictBytes(size_t n_bytes) {
  size_t evicted = 0;
  // Eviction should only occur during the Add phase, so if there are bytes in the ContiguousBuffer,
  // they are leftover from the previous iteration. If there are bytes in the ContiguousBuffer, we
  // will evict all of them as a performance optimization, since each eviction (regardless of size)
  // from the ContiguousBuffer requires copying the whole buffer. Note that because the retention
  // capacity between polling iterations is significantly lower than the DataStreamBuffer capacity,
  // the entire ContiguousBuffer is only dropped when a lot of new data was received this iteration,
  // so dropping <=1MB of data from the previous iteration doesn't seem too bad.
  // TODO(james): Look into halving the buffer each time instead of dropping the entire thing.
  if (head_ != nullptr) {
    evicted = head_->Capacity();
    head_.reset();
    head_pos_to_ts_.clear();
  }
  if (evicted >= n_bytes) {
    return evicted;
  }
  auto it = events_.begin();
  while (it != events_.end() && evicted < n_bytes) {
    size_t event_size = it->second.data.size();
    evicted += event_size;
    events_size_ -= event_size;
    it = events_.erase(it);
  }
  return evicted;
}

bool LazyContiguousDataStreamBufferImpl::IsHeadAndEventsMergeable() const {
  if (head_ == nullptr) {
    return events_size_ != 0;
  }
  // If the first event in events_ has position equal to the end of the head_ buffer, then we can
  // merge at least one event from events_ into head_.
  return events_size_ != 0 && (FirstEventPos() == head_position_ + head_->Size());
}

void LazyContiguousDataStreamBufferImpl::MergeContiguousEventsIntoHead() {
  size_t new_buffer_size = 0;
  size_t buffer_end_pos = FirstEventPos();
  size_t new_head_position = FirstEventPos();
  if (head_ != nullptr) {
    // If there is leftover data in head_ from the previous polling iteration, make sure its copied
    // into the new head.
    new_buffer_size += head_->Size();
    buffer_end_pos = head_position_ + head_->Size();
    new_head_position = head_position_;
  }

  // We first calculate how big the new buffer needs to be, and then we fill the buffer later.
  auto it = events_.begin();
  while (it != events_.end() && it->first == buffer_end_pos) {
    size_t event_size = it->second.data.size();
    new_buffer_size += event_size;
    buffer_end_pos += event_size;
    it++;
  }
  auto end_it = it;

  auto new_buffer = std::make_unique<FixedSizeContiguousBuffer>(new_buffer_size);
  size_t offset = 0;
  if (head_ != nullptr) {
    memcpy(new_buffer->Data(), head_->Data(), head_->Size());
    offset += head_->Size();
  }

  it = events_.begin();
  // end_it stopped at the first non-contiguous event (at the end of current head)
  while (it != end_it) {
    size_t event_size = it->second.data.size();
    memcpy(new_buffer->Data() + offset, it->second.data.data(), event_size);
    // Ensure that the event timestamps are monotonically increasing for a given contiguous head
    if (prev_timestamp_ > 0 && it->second.timestamp < prev_timestamp_) {
      LOG(WARNING) << absl::Substitute(
          "Detected non-monotonically increasing timestamp $0. Adjusting to previous timestamp + "
          "1: $1",
          it->second.timestamp, prev_timestamp_ + 1);
      it->second.timestamp = prev_timestamp_ + 1;
    }
    prev_timestamp_ = it->second.timestamp;
    head_pos_to_ts_.emplace(it->first, it->second.timestamp);
    offset += event_size;
    events_size_ -= event_size;
    it = events_.erase(it);
  }

  head_position_ = new_head_position;
  head_.swap(new_buffer);
}

std::string_view LazyContiguousDataStreamBufferImpl::Head() {
  if (IsHeadAndEventsMergeable()) {
    MergeContiguousEventsIntoHead();
  }
  if (head_ == nullptr) {
    return {};
  }
  return head_->StringView();
}

StatusOr<uint64_t> LazyContiguousDataStreamBufferImpl::GetTimestamp(size_t pos) const {
  // This slightly differs from the API of the AlwaysContiguousDataStreamBuffer, in that we only
  // allow getting the timestamp from the contiguous region given by Head(). This will be fine based
  // on current usage of GetTimestamp().
  if (head_ == nullptr) {
    return error::Internal("Specified position not found");
  }
  if (pos >= head_position_ + head_->Size() || pos < head_position_) {
    return error::Internal("Specified position not found");
  }
  auto it = Floor(head_pos_to_ts_, pos);
  if (it == head_pos_to_ts_.end()) {
    return error::Internal("Specified position not found");
  }
  return it->second;
}

void LazyContiguousDataStreamBufferImpl::RemovePrefix(ssize_t n) {
  DCHECK_GE(n, 0);
  if (n <= 0) {
    return;
  }
  size_t remaining = n;

  if (head_ != nullptr) {
    size_t removed = std::min(remaining, head_->Size());
    head_->RemovePrefix(removed);
    remaining -= removed;
    if (head_->Size() == 0) {
      head_.reset();
      head_pos_to_ts_.clear();
    } else {
      CleanupHeadTimestamps(removed);
    }
  }

  // If there are still bytes remaining, remove as many events as possible. If there are bytes
  // remaining to remove, but the next event is larger than the bytes remaining to remove, break out
  // of the loop and handle the last event separately.
  auto it = events_.begin();
  while (remaining > 0 && it != events_.end()) {
    size_t event_size = it->second.data.size();
    if (event_size > remaining) {
      break;
    }
    remaining -= event_size;
    events_size_ -= event_size;
    it = events_.erase(it);
  }

  // Handle remaining bytes that need to be removed from first event in events_.
  if (remaining > 0 && events_.size() > 0) {
    auto node_handle = events_.extract(events_.begin());
    node_handle.key() += remaining;
    node_handle.mapped().data = node_handle.mapped().data.substr(remaining);
    events_.insert(std::move(node_handle));
    events_size_ -= remaining;
  }

  // Increment head_position_ even if head_ became invalid. In normal operation (where `Head()` is
  // called before the next call to `position()`), this doesn't matter. However, its useful to align
  // with the old implementation's `position()` method for the purposes of tracking data loss.
  head_position_ += n;
}

void LazyContiguousDataStreamBufferImpl::CleanupHeadTimestamps(size_t removed) {
  auto it = head_pos_to_ts_.lower_bound(head_position_ + removed);
  if (it == head_pos_to_ts_.begin()) {
    return;
  }
  it--;
  head_pos_to_ts_.erase(head_pos_to_ts_.begin(), it);

  auto nh = head_pos_to_ts_.extract(it);
  nh.key() = head_position_ + removed;
  head_pos_to_ts_.insert(std::move(nh));
}

void LazyContiguousDataStreamBufferImpl::Reset() {
  head_.reset();
  head_pos_to_ts_.clear();
  events_.clear();
  events_size_ = 0;
}

void LazyContiguousDataStreamBufferImpl::ShrinkToFit() {
  if (head_ == nullptr) {
    return;
  }
  if (head_->Size() == head_->Capacity()) {
    return;
  }
  auto new_buffer = std::make_unique<FixedSizeContiguousBuffer>(head_->Size());
  memcpy(new_buffer->Data(), head_->Data(), head_->Size());
  head_.swap(new_buffer);
}

size_t LazyContiguousDataStreamBufferImpl::FirstEventPos() const {
  // This is unsafe, caller must check that there are events.
  DCHECK_GT(events_.size(), 0U);
  return events_.begin()->first;
}

size_t LazyContiguousDataStreamBufferImpl::size() const {
  if (head_ == nullptr) {
    return events_size_;
  }
  return head_->Size() + events_size_;
}

size_t LazyContiguousDataStreamBufferImpl::capacity() const {
  if (head_ == nullptr) {
    return events_size_;
  }
  return head_->Capacity() + events_size_;
}

bool LazyContiguousDataStreamBufferImpl::empty() const { return size() == 0; }

size_t LazyContiguousDataStreamBufferImpl::position() const {
  // This slightly differs from the API of the AlwaysContiguousDataStreamBuffer, in that the
  // returned position is only valid, after a call to Head().
  return head_position_;
}

std::string LazyContiguousDataStreamBufferImpl::DebugInfo() const { return ""; }

}  // namespace protocols
}  // namespace stirling
}  // namespace px
