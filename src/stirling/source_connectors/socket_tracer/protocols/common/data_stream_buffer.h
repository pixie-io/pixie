#pragma once

#include <map>
#include <string>

#include "src/common/base/base.h"

namespace pl {
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
 * TODO(oazizi): Investigate buffer that is better suited to the rolling window (slinky) model.
 */
class DataStreamBuffer {
 public:
  explicit DataStreamBuffer(size_t max_capacity) : capacity_(max_capacity) {}

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

 private:
  std::map<size_t, size_t>::const_iterator GetChunkForPos(size_t pos) const;
  void AddNewChunk(size_t pos, size_t size);
  void AddNewTimestamp(size_t pos, uint64_t timestamp);

  void CleanupTimestamps();
  void CleanupChunks();

  // Umbrella that calls CleanupTimestamps and CleanupChunks.
  void CleanupMetadata();

  const size_t capacity_;

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
}  // namespace pl
