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

#include <gflags/gflags.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/always_contiguous_data_stream_buffer_impl.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/data_stream_buffer.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/lazy_contiguous_data_stream_buffer_impl.h"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "src/common/base/base.h"

DEFINE_bool(stirling_data_stream_buffer_always_contiguous_buffer,
            gflags::BoolFromEnv("PL_STIRLING_DATA_STREAM_BUFFER_ALWAYS_CONTIGUOUS_BUFFER", true),
            "Flip flag to use alternative DataStreamBuffer implementation");

namespace px {
namespace stirling {
namespace protocols {

DataStreamBuffer::DataStreamBuffer(size_t max_capacity, size_t max_gap_size,
                                   size_t allow_before_gap_size) {
  if (FLAGS_stirling_data_stream_buffer_always_contiguous_buffer) {
    impl_ = std::unique_ptr<DataStreamBufferImpl>(new AlwaysContiguousDataStreamBufferImpl(
        max_capacity, max_gap_size, allow_before_gap_size));
  } else {
    impl_ =
        std::unique_ptr<DataStreamBufferImpl>(new LazyContiguousDataStreamBufferImpl(max_capacity));
  }
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
