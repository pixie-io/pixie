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

#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"

#include "src/common/testing/testing.h"

namespace px {
namespace stirling {
namespace protocols {

class DataStreamBufferTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    old_flag_val_ = FLAGS_stirling_data_stream_buffer_always_contiguous_buffer;
    FLAGS_stirling_data_stream_buffer_always_contiguous_buffer = GetParam();
  }
  void TearDown() override {
    FLAGS_stirling_data_stream_buffer_always_contiguous_buffer = old_flag_val_;
  }

 private:
  bool old_flag_val_;
};

TEST_P(DataStreamBufferTest, AddAndGet) {
  DataStreamBuffer stream_buffer(15, 15, 15);

  // Initially everything should be empty.
  EXPECT_EQ(stream_buffer.Head(), "");

  // Add a basic event.
  stream_buffer.Add(0, "0123", 0);
  EXPECT_EQ(stream_buffer.Head(), "0123");

  // Add an adjacent event.
  stream_buffer.Add(4, "45", 4);
  EXPECT_EQ(stream_buffer.Head(), "012345");

  // Add an event with a gap
  stream_buffer.Add(8, "89", 8);
  EXPECT_EQ(stream_buffer.Head(), "012345");

  // Fill in the gap with an out-of-order event.
  stream_buffer.Add(6, "67", 6);
  EXPECT_EQ(stream_buffer.Head(), "0123456789");

  // Fill the buffer.
  stream_buffer.Add(10, "abcde", 10);
  EXPECT_EQ(stream_buffer.Head(), "0123456789abcde");

  // Cause the buffer to expand such that data should expire.
  stream_buffer.Add(15, "fghij", 15);
  // Jump ahead, leaving a gap.
  stream_buffer.Add(28, "st", 28);
  stream_buffer.Add(26, "qr", 26);
  EXPECT_EQ(stream_buffer.Head(), "fghij");

  // Fill in the gap.
  stream_buffer.Add(22, "mn", 22);
  stream_buffer.Add(20, "kl", 20);
  stream_buffer.Add(24, "op", 24);
  EXPECT_EQ(stream_buffer.Head(), "fghijklmnopqrst");

  // Remove some of the head.
  stream_buffer.RemovePrefix(5);
  EXPECT_EQ(stream_buffer.Head(), "klmnopqrst");

  // Jump ahead such that everything should expire.
  stream_buffer.Add(100, "0123456789", 100);
  stream_buffer.Trim();
  EXPECT_EQ(stream_buffer.Head(), "0123456789");

  // Add something larger than the capacity. Head should get truncated.
  stream_buffer.Add(120, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 120);
  EXPECT_EQ(stream_buffer.Head(), "LMNOPQRSTUVWXYZ");

  // Add something way in the past. Should be ignored.
  stream_buffer.Add(50, "oldie", 50);
  EXPECT_EQ(stream_buffer.Head(), "LMNOPQRSTUVWXYZ");
}

TEST_P(DataStreamBufferTest, RemovePrefixAndTrim) {
  DataStreamBuffer stream_buffer(15, 15, 15);

  // Add some events with a gap.
  stream_buffer.Add(0, "0123", 0);
  stream_buffer.Add(10, "abcd", 10);
  EXPECT_EQ(stream_buffer.Head(), "0123");

  // Remove part of the first event.
  stream_buffer.RemovePrefix(2);
  EXPECT_EQ(stream_buffer.Head(), "23");

  // Remove more of the first event.
  stream_buffer.RemovePrefix(1);
  EXPECT_EQ(stream_buffer.Head(), "3");

  // Remove more of the first event.
  stream_buffer.RemovePrefix(1);

  // Head should have a gap, trim it.
  stream_buffer.Trim();
  EXPECT_EQ(stream_buffer.Head(), "abcd");

  // Another trim shouldn't impact anything.
  stream_buffer.Trim();
  EXPECT_EQ(stream_buffer.Head(), "abcd");

  // Removing negative amount should do nothing in production.
  // In debug mode, it should die.
  EXPECT_DEBUG_DEATH(stream_buffer.RemovePrefix(-1), "");
  EXPECT_EQ(stream_buffer.Head(), "abcd");
}

TEST_P(DataStreamBufferTest, Timestamp) {
  DataStreamBuffer stream_buffer(15, 15, 15);

  EXPECT_NOT_OK(stream_buffer.GetTimestamp(0));
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(20));

  stream_buffer.Add(0, "0123", 0);
  stream_buffer.Add(4, "4567", 4);
  stream_buffer.RemovePrefix(1);

  EXPECT_EQ(stream_buffer.Head(), "1234567");

  EXPECT_NOT_OK(stream_buffer.GetTimestamp(0));
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(1), 0);
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(3), 0);
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(4), 4);
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(7), 4);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(8));

  // Same timestamp as previous timestamp
  stream_buffer.Add(8, "89", 4);
  EXPECT_EQ(stream_buffer.Head(), "123456789");
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(8), 4);

  // Test automatic adjustment of non-monotonic timestamp
  stream_buffer.Add(10, "ab", 3);  // timestamp is 3, which is less than previous timestamp 4
  EXPECT_EQ(stream_buffer.Head(), "123456789ab");
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(10),
                   5);  // timestamp is adjusted to previous timestamp + 1
}

TEST_P(DataStreamBufferTest, TimestampWithGap) {
  DataStreamBuffer stream_buffer(15, 15, 15);

  stream_buffer.Add(0, "0123", 0);
  stream_buffer.Add(10, "abcd", 10);
  EXPECT_EQ(stream_buffer.Head(), "0123");
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(0), 0);
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(3), 0);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(4));
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(9));

  stream_buffer.RemovePrefix(2);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(0));
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(2), 0);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(9));

  stream_buffer.RemovePrefix(2);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(0));
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(9));

  stream_buffer.Trim();
  EXPECT_EQ(stream_buffer.Head(), "abcd");
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(10), 10);
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(13), 10);

  stream_buffer.RemovePrefix(2);
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(0));
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(5));
  EXPECT_NOT_OK(stream_buffer.GetTimestamp(10));
  EXPECT_OK_AND_EQ(stream_buffer.GetTimestamp(13), 10);
}

TEST_P(DataStreamBufferTest, SizeAndGetPos) {
  DataStreamBuffer stream_buffer(15, 15, 15);

  // Start off empty.
  EXPECT_EQ(stream_buffer.position(), 0);
  EXPECT_EQ(stream_buffer.size(), 0);
  EXPECT_TRUE(stream_buffer.empty());

  // Add basic event.
  stream_buffer.Add(0, "0123", 0);
  EXPECT_EQ(stream_buffer.position(), 0);
  EXPECT_EQ(stream_buffer.size(), 4);
  EXPECT_FALSE(stream_buffer.empty());

  // Add event with a gap.
  stream_buffer.Add(8, "89", 8);
  EXPECT_EQ(stream_buffer.position(), 0);
  // size() is different between the two current implementations (the new impl does not
  // include the gap in size, the old one does).
  // TODO(james): remove one of the two checks when we settle on an implementation.
  if (FLAGS_stirling_data_stream_buffer_always_contiguous_buffer) {
    EXPECT_EQ(stream_buffer.size(), 10);
  } else {
    EXPECT_EQ(stream_buffer.size(), 6);
  }
  EXPECT_FALSE(stream_buffer.empty());

  EXPECT_EQ(stream_buffer.Head(), "0123");
  // Add event that causes events to expire.
  stream_buffer.Add(20, "abcdefghijklmno", 20);
  EXPECT_EQ(stream_buffer.Head(), "abcdefghijklmno");
  EXPECT_EQ(stream_buffer.position(), 20);
  EXPECT_EQ(stream_buffer.size(), 15);
  EXPECT_FALSE(stream_buffer.empty());

  // Remove prefix should shrink size.
  stream_buffer.RemovePrefix(1);
  EXPECT_EQ(stream_buffer.position(), 21);
  EXPECT_EQ(stream_buffer.size(), 14);
  EXPECT_FALSE(stream_buffer.empty());

  // Can even shrink to zero.
  stream_buffer.RemovePrefix(14);
  EXPECT_EQ(stream_buffer.size(), 0);
  EXPECT_TRUE(stream_buffer.empty());

  // Add something larger than the capacity.
  stream_buffer.Add(105, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 105);
  EXPECT_EQ(stream_buffer.size(), 15);
  EXPECT_FALSE(stream_buffer.empty());
}

TEST_P(DataStreamBufferTest, LargeGap) {
  const size_t kMaxGapSize = 32;
  const size_t kAllowBeforeGapSize = 8;
  DataStreamBuffer stream_buffer(128, kMaxGapSize, kAllowBeforeGapSize);

  // Add basic event.
  stream_buffer.Add(0, "0123", 0);
  EXPECT_EQ(stream_buffer.position(), 0);
  EXPECT_EQ(stream_buffer.size(), 4);
  EXPECT_FALSE(stream_buffer.empty());

  // Add event with gap less than max_gap_size.
  stream_buffer.Add(32, "4567", 10);
  EXPECT_EQ(stream_buffer.Head(), "0123");

  // Add event with gap larger than max_gap_size.
  stream_buffer.Add(100, "abcd", 20);

  // These tests only apply to the old implementation, the new implementation will keep all of this
  // data in its buffer, since it doesn't allocate gaps.
  // TODO(james): remove when we settle on an implementation.
  if (FLAGS_stirling_data_stream_buffer_always_contiguous_buffer) {
    EXPECT_EQ(stream_buffer.size(), 4 + kAllowBeforeGapSize);

    // Add event more than allow_before_gap_size before the last event. This event should not be
    // added to the buffer.
    stream_buffer.Add(100 - kMaxGapSize, "test", 18);
    EXPECT_EQ(stream_buffer.size(), 4 + kAllowBeforeGapSize);

    // Add event before the gap event but not more than allow_before_gap_size before. This event
    // should be added to the buffer.
    stream_buffer.Add(100 - kAllowBeforeGapSize, "allow", 19);
    EXPECT_EQ(stream_buffer.Head(), "allow");
  }
}

INSTANTIATE_TEST_SUITE_P(DataStreamBufferImplTest, DataStreamBufferTest,
                         ::testing::Values(true, false),
                         [](const ::testing::TestParamInfo<DataStreamBufferTest::ParamType>& info) {
                           if (info.param) {
                             return "AlwaysContiguousImpl";
                           } else {
                             return "LazyContiguousImpl";
                           }
                         });

}  // namespace protocols
}  // namespace stirling
}  // namespace px
