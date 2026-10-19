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
  VLOG(1) << "Resetting AlwaysContiguousDataStreamBufferImpl.";
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
    size_t r_size = r_iter->second.size;

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
    size_t l_size = l_iter->second.size;

    left_overlap = (pos < l_pos + l_size);
    ECHECK(!left_overlap) << absl::Substitute(
        "New chunk overlaps with left chunk. "
        "Existing left chunk: [p=$0,s=$1] New Chunk: [p=$2,s=$3]",
        l_pos, l_size, pos, size);
  }

  return left_overlap || right_overlap;
}

void AlwaysContiguousDataStreamBufferImpl::AddNewChunk(size_t pos, size_t size,
                                                       chunk_t incomplete_chunk, size_t gap_size) {
  // Look for the chunks to the left and right of this new chunk.
  auto r_iter = chunks_.lower_bound(pos);
  auto l_iter = r_iter;
  if (l_iter != chunks_.begin()) {
    --l_iter;
  }

  if (incomplete_chunk == chunk_t::kSendFile) {
    // SendFile is a special case where the data is not actually in the buffer.
    // We just need to update the chunk_info metadata for closest chunk so that the filler
    // event is processed properly (if lazy parsing is off).
    // If pos is at the end of l_iter, then we need to update r_iter's chunk_info.
    DCHECK_EQ(size, 0);  // SendFile events should have size 0 because we don't track them in bpf
    DCHECK_GE(gap_size, 0);  // SendFile events should have a non-zero gap size
    DCHECK_EQ(l_iter->first + l_iter->second.size, pos);
    VLOG(1) << absl::Substitute("l_iter is [$0, $1]", l_iter->first,
                                l_iter->first + l_iter->second.size);
    VLOG(1) << absl::Substitute("pos is at the end of l_iter, so updating l_iter's chunk_info");
    // pos is at the end of l_iter, so update l_iter's chunk_info.
    l_iter->second.AddIncompleteChunkInfo(
        IncompleteChunkInfo(incomplete_chunk, pos, gap_size, pos));
    VLOG(1) << absl::Substitute("l_iter now has incomplete_chunk: $0, at [$1, $2]",
                                l_iter->second.MostRecentIncompleteChunkInfo().incomplete_chunk,
                                l_iter->second.MostRecentIncompleteChunkInfo().gap_start,
                                l_iter->second.MostRecentIncompleteChunkInfo().gap_start +
                                    l_iter->second.MostRecentIncompleteChunkInfo().gap_size);
    return;
  }

  // Does this chunk fuse with the chunk on the left of it?
  bool left_fuse = (l_iter != chunks_.end() && l_iter->first + l_iter->second.size == pos);
  // Does this chunk fuse with the chunk on the right of it?
  bool right_fuse = (r_iter != chunks_.end() && pos + size == r_iter->first);

  if (left_fuse && right_fuse) {
    // The new chunk bridges two previously separate chunks together.
    // Keep the left one and increase its size to cover all three chunks.
    l_iter->second.size += (size + r_iter->second.size);

    // Left and right chunks must have been added prior to the middle chunk, so either
    // 1. left chunk has no incomplete chunks
    // 2. the most recent incomplete chunk has a filled gap,
    // 3. or the new chunk is a filler event that completely plugs the gap of the left chunk.
    DCHECK(!l_iter->second.HasIncompleteChunks() ||
           l_iter->second.MostRecentIncompleteChunkInfo().gap_filled ||
           incomplete_chunk == chunk_t::kFiller);

    // Middle chunk cannot be incomplete, because it would have been added before the right chunk
    // (which must be filler if the middle chunk is incomplete)
    DCHECK(incomplete_chunk == chunk_t::kFullyFormed || incomplete_chunk == chunk_t::kFiller ||
           incomplete_chunk == chunk_t::kHeaderEvent);

    // New chunk is a filler event that plugs the gap of the left incomplete chunk.
    if (incomplete_chunk == chunk_t::kFiller) {
      DCHECK(l_iter->second.HasIncompleteChunks());
      DCHECK_EQ(l_iter->second.MostRecentIncompleteChunkInfo().gap_start, pos);
      DCHECK_EQ(l_iter->second.MostRecentIncompleteChunkInfo().gap_size, size);
      DCHECK(!l_iter->second.MostRecentIncompleteChunkInfo().gap_filled);
      // Mark the gap as filled: gap_start is pos, while gap_end should be pos + gap_size, or the
      // size of this filler.
      l_iter->second.MostRecentIncompleteChunkInfo().MarkGapAsFilled(pos, pos + size);
      DCHECK(l_iter->second.MostRecentIncompleteChunkInfo().gap_filled);
    }

    // If right chunk is incomplete, the merged chunk (new left) should inherit its
    // incompleteness.
    if (r_iter->second.HasIncompleteChunks()) {
      for (const auto& incomplete_chunk_info : r_iter->second.incomplete_chunks) {
        l_iter->second.AddIncompleteChunkInfo(incomplete_chunk_info);
      }
    }
    // If all chunks complete, then the merged chunk is complete
    chunks_.erase(r_iter);  // Remove the right chunk since it will be merged into left.
  } else if (left_fuse) {
    // Merge new chunk directly to the one on its left.
    VLOG(1) << absl::Substitute("Merging new chunk [$0, $1] into left chunk [$2, $3]", pos,
                                pos + size, l_iter->first, l_iter->first + l_iter->second.size);
    l_iter->second.size += size;
    // 1. If the left chunk is incomplete with an unfilled gap starting at pos, this new chunk must
    // be a filler event that partially or fully plugs the gap.
    if (l_iter->second.HasIncompleteChunks() &&
        l_iter->second.MostRecentIncompleteChunkInfo().gap_start == pos) {
      VLOG(1) << absl::Substitute("Left chunk has incomplete_chunk: $0, gap_size: $1",
                                  l_iter->second.MostRecentIncompleteChunkInfo().incomplete_chunk,
                                  l_iter->second.MostRecentIncompleteChunkInfo().gap_size);
      DCHECK(incomplete_chunk == chunk_t::kFiller ||
             incomplete_chunk == chunk_t::kIncompleteFiller);
      DCHECK_NE(l_iter->second.MostRecentIncompleteChunkInfo().gap_size, 0);
      if (size == l_iter->second.MostRecentIncompleteChunkInfo().gap_size) {
        // This filler event completely fills the gap of the incomplete chunk.
        DCHECK_EQ(size, l_iter->second.MostRecentIncompleteChunkInfo().gap_size);
        VLOG(1) << absl::Substitute(
            "New chunk is a filler event that completely fills the left chunk's gap.");
        // Mark the gap as filled: gap_start is pos, while gap_end should be pos + gap_size, or the
        // size of this filler.
        l_iter->second.MostRecentIncompleteChunkInfo().MarkGapAsFilled(pos, pos + size);
        DCHECK(l_iter->second.MostRecentIncompleteChunkInfo().gap_filled);
      } else {
        // This filler event only partially fills the gap of the incomplete chunk.
        // Add a new incomplete chunk info
        l_iter->second.AddIncompleteChunkInfo(IncompleteChunkInfo(
            chunk_t::kIncompleteFiller, pos,
            l_iter->second.MostRecentIncompleteChunkInfo().gap_size - size, pos + size));
        VLOG(1) << absl::Substitute(
            "New chunk is an incomplete filler event that only partially fills the left chunk's "
            "gap.");
      }
    }

    // 2. If the new chunk is incomplete (and not filler), the left chunk must be complete or an
    // incomplete chunk with a filled gap.
    if (incomplete_chunk != chunk_t::kFullyFormed && incomplete_chunk != chunk_t::kFiller &&
        incomplete_chunk != chunk_t::kIncompleteFiller &&
        incomplete_chunk != chunk_t::kHeaderEvent) {
      DCHECK_GE(gap_size, 0);  // new incomplete chunk must have a gap
      DCHECK(!l_iter->second.HasIncompleteChunks() ||
             l_iter->second.MostRecentIncompleteChunkInfo().gap_filled);
      // The merged chunk (new left) should inherit the new chunk's incompleteness.
      l_iter->second.AddIncompleteChunkInfo(
          IncompleteChunkInfo(incomplete_chunk, pos, gap_size, pos + size));
      VLOG(1) << absl::Substitute(
          "New chunk has size: $0, gap_start: $1, incomplete_chunk: $2, gap_size: $3", size,
          l_iter->second.MostRecentIncompleteChunkInfo().gap_start, incomplete_chunk, gap_size);
    }
  } else if (right_fuse) {
    // Merge new chunk into the one on its right.
    VLOG(1) << absl::Substitute("Merging new chunk [$0, $1] into right chunk [$2, $3]", pos,
                                pos + size, r_iter->first, r_iter->first + r_iter->second.size);
    // 1. New chunk cannot be incomplete, because the right would have to be a filler event added
    // after it nor can it be filler, because then it would merge into an incomplete chunk on its
    // left
    DCHECK(incomplete_chunk == chunk_t::kFullyFormed || incomplete_chunk == chunk_t::kFiller);
    // 2. If right chunk is incomplete, the merged chunk should inherit its incompleteness.
    if (r_iter->second.HasIncompleteChunks()) {
      VLOG(1) << absl::Substitute("Right chunk has incomplete_chunk: $0, gap_size: $1",
                                  r_iter->second.MostRecentIncompleteChunkInfo().incomplete_chunk,
                                  r_iter->second.MostRecentIncompleteChunkInfo().gap_size);
    }
    // Since its key changes, this requires removing and re-inserting the node.
    auto node = chunks_.extract(r_iter);  // transfers ChunkInfo from right chunk to merged chunk
    node.key() = pos;
    node.mapped().size += size;
    chunks_.insert(std::move(node));
  } else {
    // No fusing, so just add the new chunk.
    VLOG(1) << absl::Substitute("Adding new chunk at pos: [$0, $1]", pos, pos + size);
    ChunkInfo new_chunk_info(size);

    // If the new chunk is incomplete (i.e. has a gap) we take note of the gap size and the start of
    // the incomplete event.
    if (incomplete_chunk != kFullyFormed && incomplete_chunk != kHeaderEvent) {
      DCHECK_GE(gap_size, 0);
      size_t incomplete_event_start = pos;  // start of the incomplete event (not the gap or filler)
      size_t gap_start = pos + size;        // start of the gap at the end of this chunk
      IncompleteChunkInfo new_incomplete_chunk_info(incomplete_chunk, incomplete_event_start,
                                                    gap_size, gap_start);
      new_chunk_info.AddIncompleteChunkInfo(new_incomplete_chunk_info);
      VLOG(1) << absl::Substitute("New chunk has incomplete_chunk: $0, gap_size: $1, size: $2",
                                  incomplete_chunk, gap_size, size);
    }
    chunks_.emplace(pos, new_chunk_info);
  }
}

void AlwaysContiguousDataStreamBufferImpl::AddNewTimestamp(size_t pos, uint64_t timestamp) {
  timestamps_[pos] = timestamp;
}

void AlwaysContiguousDataStreamBufferImpl::Add(size_t pos, std::string_view data,
                                               uint64_t timestamp, chunk_t incomplete_chunk,
                                               size_t gap_size) {
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
    LOG(WARNING) << absl::Substitute(
        "Not adding chunk at pos $0, size $1, because it overlaps with an existing chunk.", pos,
        data.size());
    return;
  }

  // Now copy the data into the buffer.
  memcpy(buffer_.data() + ppos_front, data.data(), data.size());

  // Update the metadata.
  AddNewChunk(pos, data.size(), incomplete_chunk, gap_size);
  AddNewTimestamp(pos, timestamp);

  if (run_metadata_cleanup) {
    CleanupMetadata();
  }
}

std::map<size_t, ChunkInfo>::const_iterator AlwaysContiguousDataStreamBufferImpl::GetChunkForPos(
    size_t pos) const {
  // Get chunk which is <= pos.
  auto iter = MapLE(chunks_, pos);
  if (iter == chunks_.cend()) {
    return chunks_.cend();
  }

  DCHECK_GE(pos, iter->first);

  // Does the chunk include pos? If not, return {}.
  ssize_t available = iter->second.size - (pos - iter->first);
  if (available <= 0) {
    return chunks_.cend();
  }

  return iter;
}

ChunkInfo AlwaysContiguousDataStreamBufferImpl::GetChunkInfoForHead() {
  auto iter = GetChunkForPos(position_);

  if (iter == chunks_.cend()) {
    return ChunkInfo(0);
  }
  return iter->second;
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

  ChunkInfo chunk_info = iter->second;
  size_t chunk_pos = iter->first;
  size_t chunk_size = chunk_info.size;

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
  size_t chunk_size = iter->second.size;

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
    node.mapped().size = available;
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
    end_position = last_chunk->first + last_chunk->second.size;
  }
  return end_position;
}

std::string AlwaysContiguousDataStreamBufferImpl::DebugInfo() const {
  std::string s;

  absl::StrAppend(&s, absl::Substitute("Position: $0\n", position_));
  absl::StrAppend(&s, absl::Substitute("BufferSize: $0/$1\n", buffer_.size(), capacity_));
  absl::StrAppend(&s, "Chunks:\n");
  for (const auto& [pos, chunk_info] : chunks_) {
    absl::StrAppend(&s, absl::Substitute("  position:$0 size:$1 incomplete_chunks:$2", pos,
                                         chunk_info.size, chunk_info.incomplete_chunks.size()));
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
