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

#include "src/stirling/source_connectors/socket_tracer/protocols/common/always_contiguous_data_stream_buffer_impl.h"

namespace px {
namespace stirling {
namespace protocols {

namespace {

// Get element <= key in a map.
template <typename TMapType>
typename TMapType::const_iterator MapLE(const TMapType& map, size_t key) {
  auto iter = map.upper_bound(key);
  if (iter == map.begin()) {
    return map.cend();
  }
  --iter;

  return iter;
}

}  // namespace

void AlwaysContiguousDataStreamBufferImpl::Reset() {
  buffer_.clear();
  chunks_.clear();
  timestamps_.clear();
  position_ = 0;
  ShrinkToFit();
}

bool AlwaysContiguousDataStreamBufferImpl::CheckOverlap(size_t pos, size_t size) {
  bool left_overlap = false;
  bool right_overlap = false;

  // Look for the first chunk whose start position is to the right
  // of start position of this new chunk.
  auto r_iter = chunks_.lower_bound(pos);

  if (r_iter != chunks_.end()) {
    // There is a chunk to the right. Check for overlap.
    size_t r_pos = r_iter->first;
    size_t r_size = r_iter->second;

    right_overlap = (pos + size > r_pos);
    ECHECK(!right_overlap) << absl::Substitute(
        "New chunk overlaps with right chunk. "
        "Existing right chunk: [p=$0,s=$1] New Chunk: [p=$2,s=$3]",
        r_pos, r_size, pos, size);
  }

  // Find the chunk right before the right chunk. If it exists, this should
  // be our left chunk.
  auto l_iter = r_iter;
  if (l_iter != chunks_.begin()) {
    --l_iter;
    // There is a chunk to the left. Check for overlap.
    size_t l_pos = l_iter->first;
    size_t l_size = l_iter->second;

    left_overlap = (pos < l_pos + l_size);
    ECHECK(!left_overlap) << absl::Substitute(
        "New chunk overlaps with left chunk. "
        "Existing left chunk: [p=$0,s=$1] New Chunk: [p=$2,s=$3]",
        l_pos, l_size, pos, size);
  }

  return left_overlap || right_overlap;
}

void AlwaysContiguousDataStreamBufferImpl::AddNewChunk(size_t pos, size_t size) {
  // Look for the chunks to the left and right of this new chunk.
  auto r_iter = chunks_.lower_bound(pos);
  auto l_iter = r_iter;
  if (l_iter != chunks_.begin()) {
    --l_iter;
  }

  // Does this chunk fuse with the chunk on the left of it?
  bool left_fuse = false;
  if (l_iter != chunks_.end()) {
    size_t l_pos = l_iter->first;
    size_t l_size = l_iter->second;

    left_fuse = (l_pos + l_size == pos);
  }

  // Does this chunk fuse with the chunk on the right of it?
  bool right_fuse = false;
  if (r_iter != chunks_.end()) {
    size_t r_pos = r_iter->first;

    right_fuse = (pos + size == r_pos);
  }

  if (left_fuse && right_fuse) {
    // The new chunk bridges two previously separate chunks together.
    // Keep the left one and increase its size to cover all three chunks.
    l_iter->second += (size + r_iter->second);
    chunks_.erase(r_iter);
  } else if (left_fuse) {
    // Merge new chunk directly to the one on its left.
    l_iter->second += size;
  } else if (right_fuse) {
    // Merge new chunk into the one on its right.
    // Since its key changes, this requires removing and re-inserting the node.
    auto node = chunks_.extract(r_iter);
    node.key() = pos;
    node.mapped() += size;
    chunks_.insert(std::move(node));
  } else {
    // No fusing, so just add the new chunk.
    chunks_[pos] = size;
  }
}

void AlwaysContiguousDataStreamBufferImpl::AddNewTimestamp(size_t pos, uint64_t timestamp) {
  timestamps_[pos] = timestamp;
}

void AlwaysContiguousDataStreamBufferImpl::Add(size_t pos, std::string_view data,
                                               uint64_t timestamp) {
  if (data.size() > capacity_) {
    size_t oversize_amount = data.size() - capacity_;
    data.remove_prefix(oversize_amount);
    pos += oversize_amount;
  }

  // Calculate physical positions (ppos) where the data would live in the physical buffer.
  ssize_t ppos_front = pos - position_;
  ssize_t ppos_back = pos + data.size() - position_;

  bool run_metadata_cleanup = false;

  if (ppos_back <= 0) {
    // Case 1: Data being added is too far back. Just ignore it.

    // This has been observed to happen a lot on initial deployment,
    // where a large batch of events, with cumulative size greater than the buffer size
    // arrive in scrambled order.
    VLOG(1) << absl::Substitute(
        "Ignoring event that has already been skipped [event pos=$0, current pos=$1].", pos,
        position_);
    return;
  } else if (ppos_front < 0) {
    // Case 2: Data being added is straddling the front-side of the buffer. Cut-off the prefix.

    VLOG(1) << absl::Substitute(
        "Event is partially too far in the past [event pos=$0, current pos=$1].", pos, position_);

    ssize_t prefix = 0 - ppos_front;
    data.remove_prefix(prefix);
    pos += prefix;
    ppos_front = 0;
  } else if (ppos_back > static_cast<ssize_t>(buffer_.size())) {
    // Case 3: Data being added extends the buffer. Resize the buffer.

    // Subcase: If the data is more than max_gap_size_ ahead of the last chunk in the buffer (or
    // more than max_gap_size_ ahead of position_ if there aren't any chunks) than we give up on the
    // data currently in the buffer, and stick the new data `allow_before_gap_size_` bytes from the
    // front of the buffer. This allows `allow_before_gap_size_` bytes of new data to come in out of
    // order after this event that caused the gap.
    if (pos > EndPosition() + max_gap_size_) {
      position_ = pos - allow_before_gap_size_;
      ppos_front = allow_before_gap_size_;
      ppos_back = allow_before_gap_size_ + data.size();
      // Delay cleaning up metadata until after adding the new chunk, so that `CleanupMetadata`'s
      // view of buffer_ and chunks_ is not in an intermediate state.
      run_metadata_cleanup = true;
    }

    if (pos > position_ + capacity_) {
      // This has been observed to happen a lot on initial deployment,
      // where a large batch of events, with cumulative size greater than the buffer size
      // arrive in scrambled order.
      VLOG(1) << absl::Substitute("Event skips ahead *a lot* [event pos=$0, current pos=$1].", pos,
                                  position_);
    }

    ssize_t logical_size = pos + data.size() - position_;
    if (logical_size > static_cast<ssize_t>(capacity_)) {
      // The movement of the buffer position will cause some bytes to "fall off",
      // remove those now.
      size_t remove_count = logical_size - capacity_;

      VLOG(1) << absl::Substitute("Event bytes to be dropped [count=$0].", remove_count);

      RemovePrefix(remove_count);
      ppos_front -= remove_count;
      ppos_back -= remove_count;
    }

    DCHECK_GE(ppos_front, 0);
    DCHECK_LE(ppos_front, static_cast<ssize_t>(capacity_));

    DCHECK_GE(ppos_back, 0);
    DCHECK_LE(ppos_back, static_cast<ssize_t>(capacity_));

    ssize_t new_size = ppos_back;
    DCHECK_LE(new_size, static_cast<ssize_t>(capacity_));
    DCHECK_GE(new_size, 0);

    buffer_.resize(new_size);

    DCHECK_GE(buffer_.size(), 0U);
    DCHECK_LE(buffer_.size(), capacity_);
  } else {
    // Case 4: Data being added is completely within the buffer. Write it directly.

    // No adjustments required.
  }

  if (CheckOverlap(pos, data.size())) {
    // This chunk overlaps with an existing chunk. Don't add the new chunk.
    return;
  }

  // Now copy the data into the buffer.
  memcpy(buffer_.data() + ppos_front, data.data(), data.size());

  // Update the metadata.
  AddNewChunk(pos, data.size());
  AddNewTimestamp(pos, timestamp);

  if (run_metadata_cleanup) {
    CleanupMetadata();
  }
}

std::map<size_t, size_t>::const_iterator AlwaysContiguousDataStreamBufferImpl::GetChunkForPos(
    size_t pos) const {
  // Get chunk which is <= pos.
  auto iter = MapLE(chunks_, pos);
  if (iter == chunks_.cend()) {
    return chunks_.cend();
  }

  DCHECK_GE(pos, iter->first);

  // Does the chunk include pos? If not, return {}.
  ssize_t available = iter->second - (pos - iter->first);
  if (available <= 0) {
    return chunks_.cend();
  }

  return iter;
}

void AlwaysContiguousDataStreamBufferImpl::EnforceTimestampMonotonicity(size_t pos,
                                                                        size_t chunk_end) {
  // Get timestamp for chunk which is <= pos.
  auto it = timestamps_.upper_bound(pos);
  if (it == timestamps_.begin()) {
    return;
  }
  --it;

  // Loop from chunk_start up to but not including chunk_end.
  // The next element after chunk_end should not be part of the contiguous block.
  prev_timestamp_ = 0;
  for (; it != timestamps_.end() && it->first < chunk_end; ++it) {
    if (prev_timestamp_ > 0 && it->second < prev_timestamp_) {
      LOG(WARNING) << absl::Substitute(
          "For chunk pos $0, detected non-monotonically increasing timestamp $1. Adjusting to "
          "previous timestamp + 1: $2",
          it->first, it->second, prev_timestamp_ + 1);
      it->second = prev_timestamp_ + 1;
    }
    prev_timestamp_ = it->second;
  }
}

std::string_view AlwaysContiguousDataStreamBufferImpl::Get(size_t pos) {
  auto iter = GetChunkForPos(pos);
  if (iter == chunks_.cend()) {
    return {};
  }

  size_t chunk_pos = iter->first;    // start of contiguous head
  size_t chunk_size = iter->second;  // size of contiguous (already merged in Add)

  // since we only call Get() in Head() and the event parser fully processes a contiguous head,
  // we need only enforce timestamp monotonicity once per head.
  EnforceTimestampMonotonicity(chunk_pos, chunk_pos + chunk_size);

  ssize_t bytes_available = chunk_size - (pos - chunk_pos);
  DCHECK_GT(bytes_available, 0);

  DCHECK_GE(pos, position_);
  size_t ppos = pos - position_;
  DCHECK_LT(ppos, buffer_.size());
  return std::string_view(buffer_.data() + ppos, bytes_available);
}

StatusOr<uint64_t> AlwaysContiguousDataStreamBufferImpl::GetTimestamp(size_t pos) const {
  // Ensure the specified time corresponds to a real chunk.
  if (GetChunkForPos(pos) == chunks_.cend()) {
    return error::Internal("Specified position not found");
  }

  // Get chunk which is <= pos.
  auto iter = MapLE(timestamps_, pos);
  if (iter == timestamps_.cend()) {
    LOG(DFATAL) << absl::Substitute(
        "Specified position should have been found, since we verified we are not in a chunk gap "
        "[position=$0]\n$1.",
        pos, DebugInfo());
    return error::Internal("Specified position not found.");
  }

  DCHECK_GE(pos, iter->first);

  return iter->second;
}

void AlwaysContiguousDataStreamBufferImpl::CleanupMetadata() {
  CleanupChunks();
  CleanupTimestamps();
}

void AlwaysContiguousDataStreamBufferImpl::CleanupChunks() {
  // Find and remove irrelevant metadata in `chunks_`.

  // Get chunk which is <= position_.
  auto iter = MapLE(chunks_, position_);
  if (iter == chunks_.cend()) {
    return;
  }

  size_t chunk_pos = iter->first;
  size_t chunk_size = iter->second;

  DCHECK_GE(position_, chunk_pos);
  ssize_t available = chunk_size - (position_ - chunk_pos);

  if (available <= 0) {
    // position_ was in a gap area between two chunks, so go back to the next chunk.
    ++iter;
    chunks_.erase(chunks_.begin(), iter);
  } else {
    // Remove all chunks entirely before position_.
    chunks_.erase(chunks_.begin(), iter);

    // Adjust the first chunk's size.
    DCHECK(!chunks_.empty());
    auto node = chunks_.extract(chunks_.begin());
    node.key() = position_;
    node.mapped() = available;
    chunks_.insert(std::move(node));
  }

  if (chunks_.empty()) {
    ECHECK(buffer_.empty()) << "Invalid state in AlwaysContiguousDataStreamBufferImpl. "
                               "buffer_ is non-empty, but chunks_ is empty.";
    buffer_.clear();
  }
}

void AlwaysContiguousDataStreamBufferImpl::CleanupTimestamps() {
  // Find and remove irrelevant metadata in `timestamps_`.

  // Get timestamp which is <= position_.
  auto iter = MapLE(timestamps_, position_);
  if (iter == timestamps_.cend()) {
    return;
  }

  // We are now at the timestamp that covers position_,
  // anything before this is expired and can be removed.
  timestamps_.erase(timestamps_.begin(), iter);

  DCHECK(!timestamps_.empty());
}

void AlwaysContiguousDataStreamBufferImpl::RemovePrefix(ssize_t n) {
  // Check for positive values of n.
  // For safety in production code, just return.
  DCHECK_GE(n, 0);
  if (n < 0) {
    return;
  }

  buffer_.erase(0, n);
  position_ += n;

  CleanupMetadata();
}

void AlwaysContiguousDataStreamBufferImpl::Trim() {
  if (chunks_.empty()) {
    return;
  }

  auto& chunk_pos = chunks_.begin()->first;
  DCHECK_GE(chunk_pos, position_);
  size_t trim_size = chunk_pos - position_;

  buffer_.erase(0, trim_size);
  position_ += trim_size;
}

size_t AlwaysContiguousDataStreamBufferImpl::EndPosition() {
  size_t end_position = position_;
  if (!chunks_.empty()) {
    auto last_chunk = std::prev(chunks_.end());
    end_position = last_chunk->first + last_chunk->second;
  }
  return end_position;
}

std::string AlwaysContiguousDataStreamBufferImpl::DebugInfo() const {
  std::string s;

  absl::StrAppend(&s, absl::Substitute("Position: $0\n", position_));
  absl::StrAppend(&s, absl::Substitute("BufferSize: $0/$1\n", buffer_.size(), capacity_));
  absl::StrAppend(&s, "Chunks:\n");
  for (const auto& [pos, size] : chunks_) {
    absl::StrAppend(&s, absl::Substitute("  position:$0 size:$1\n", pos, size));
  }
  absl::StrAppend(&s, "Timestamps:\n");
  for (const auto& [pos, timestamp] : timestamps_) {
    absl::StrAppend(&s, absl::Substitute("  position:$0 timestamp:$1\n", pos, timestamp));
  }
  absl::StrAppend(&s, absl::Substitute("Buffer: $0\n", buffer_));

  return s;
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
