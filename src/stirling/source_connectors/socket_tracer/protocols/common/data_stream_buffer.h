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
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

DECLARE_bool(stirling_data_stream_buffer_always_contiguous_buffer);

namespace px {
namespace stirling {
namespace protocols {

// IncompleteChunkInfo keeps track of the chunks we know to be incomplete from bpf for the
// DataStreamBuffer. This metadata is updated during merging of contiguous events in Add(). If gaps
// are present, we should tell the event parser to treat this data as incomplete and lazily parse as
// many bytes as possible. Each contiguous section (without filler) will have at most one incomplete
// chunk that ends with a gap, containing n >= 0 valid frames and at most one partial frame. For
// example:
//
//      event   event   incomplete_event
//        v       v       v
//  _______________________________________
// |      |       |       |       |       |
// |      |       |       |       |       |
// |      |       |       |       |       |
// |      |       |       |       |       |
// |      |       |       |       |  gap  |
// |      |       |       |       |       |
// |      |       |       |       |       |
// |      |       |       |       |       |
// |______|_______|_______|_______|_______|
//     n valid frames     ^ n valid frames and at most 1 partial frame in this chunk before the gap
//
// Chunks grow when adjacent events are merged, so we keep track of the start position of the event
// for which we know there is an upcoming gap. The gap start in these cases is the end of head_.
//
// ------- Chained Incomplete Chunks -------
// We currently add filler \0 bytes to the end of incomplete events (up to 1MB), so it is possible
// that a filler event will bridge the gap between two incomplete events.
//
// incomplete_event     incomplete_event     event
// v                    v                     v
// ____________________________________________________
// |         |          |          |          |       |
// |         |          |          |          |       |
// |         |          |          |          |       |
// |         |    gap   |          |    gap   |       |
// |         |  filled  |          |  filled  |       |
// |         |   with   |          |   with   |       |
// |         |    \0    |          |    \0    |       |
// |         |          |          |          |       |
// |_________|__________|__________|__________|_______|
// ^                    ^                     ^ n valid frames
//  n valid frames and at most
//  1 partial frame in each incomplete chunk before the filled gaps.

// Because of this, we may have any number of incomplete events joined together by filler in
// a contiguous section of the buffer. If the filler event fails to plug the entire gap
// (i.e. the gap is larger than 1MB), then we end up with an actual gap.
struct IncompleteChunkInfo {
  // What is the reason for the gap (should not be kFullyFormed)
  chunk_t incomplete_chunk;  // todo: rename to reason, also rename event->incomplete_chunk to
                             // chunk_type

  // Where does the event start for which we know there is an upcoming gap
  size_t incomplete_event_start;

  // How large is the gap (whether filled or not)
  size_t gap_size;

  // Where does the gap start (should be the end of the contiguous section if unfilled)
  size_t gap_start;

  // If the gap is filled, where does the gap end
  // TODO(@benkilimnik): consider making this gap_start + gap_size instead of std::optional
  std::optional<size_t> gap_end = std::nullopt;

  // Was this gap plugged by filler \0 bytes
  bool gap_filled;

  // Bytes rendered unparseable because they were cut off by the gap (filled or not)
  size_t unparseable_bytes_before_gap = 0;

  void MarkGapAsFilled(size_t filler_start, size_t filler_end) {
    DCHECK(filler_start == gap_start);
    gap_filled = true;
    this->gap_end = filler_end;
  }

  IncompleteChunkInfo(chunk_t inc, size_t inc_event_start, size_t gap_size, size_t gap_start,
                      bool gap_filled = false)
      : incomplete_chunk(inc),
        incomplete_event_start(inc_event_start),
        gap_size(gap_size),
        gap_start(gap_start),
        gap_filled(gap_filled) {
    DCHECK(inc != chunk_t::kFullyFormed);
  }

  ~IncompleteChunkInfo() {
    LOG(WARNING) << absl::Substitute(
        "IncompleteChunkInfo destructor called for incomplete chunk of type $0, gap located at "
        "[$1, $2] with $3 bytes of unparseable data before the gap",
        incomplete_chunk, gap_start, gap_start + gap_size, unparseable_bytes_before_gap);
  }
};
struct ChunkInfo {
  size_t size;
  // A contiguous section of the buffer may have multiple incomplete chunks joined together by
  // filler.
  // TODO(benkilimnik): eventually, we should replace filler events with lazy parsing for all
  // protocols. At that point, we can remove the vector below because we will only have one
  // incomplete chunk per contiguous section.
  std::vector<IncompleteChunkInfo> incomplete_chunks = {};

  void AddIncompleteChunkInfo(IncompleteChunkInfo inc) { incomplete_chunks.emplace_back(inc); }

  // Rightmost in the buffer
  IncompleteChunkInfo& MostRecentIncompleteChunkInfo() {
    DCHECK(!incomplete_chunks.empty());
    return incomplete_chunks.back();
  }

  // Leftmost in the buffer
  IncompleteChunkInfo& OldestIncompleteChunkInfo() {
    DCHECK(!incomplete_chunks.empty());
    return incomplete_chunks.front();
  }

  // Sum of all gaps/filler in this contiguous chunk
  size_t TotalGapSize() const {
    size_t total_gap_size = 0;
    for (const auto& inc : incomplete_chunks) {
      total_gap_size += inc.gap_size;
    }
    return total_gap_size;
  }

  bool HasIncompleteChunks() const { return !incomplete_chunks.empty(); }

  explicit ChunkInfo(size_t sz = 0) : size(sz) {}
};

// TODO(james): switch back to concrete DataStreamBuffer (or standard abstract class) once we've
// settled on a DataStreamBuffer implementation.
class DataStreamBufferImpl {
 public:
  virtual ~DataStreamBufferImpl() = default;
  virtual void Add(size_t pos, std::string_view data, uint64_t timestamp,
                   chunk_t incomplete_chunk = chunk_t::kFullyFormed, size_t gap_size = 0) = 0;
  virtual std::string_view Head() = 0;
  virtual ChunkInfo GetChunkInfoForHead() = 0;
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
  void Add(size_t pos, std::string_view data, uint64_t timestamp,
           chunk_t incomplete_chunk = chunk_t::kFullyFormed, size_t gap_size = 0) {
    impl_->Add(pos, data, timestamp, incomplete_chunk, gap_size);
  }

  /**
   * Get all the contiguous data at the head of the buffer.
   * @return A string_view to the data.
   */
  std::string_view Head() { return impl_->Head(); }

  /**
   * Get the chunk info for the head of the buffer.
   * @return The chunk info.
   */
  ChunkInfo GetChunkInfoForHead() { return impl_->GetChunkInfoForHead(); }

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
