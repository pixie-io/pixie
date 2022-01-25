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
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"

namespace px {
namespace stirling {
namespace testing {

// Base class for all record generators. Subclass should implement `Next()` to return the next
// generated record.
class RecordGenerator {
 public:
  // We represent a generated record as a list of frames for that records, with corresponding
  // traffic direction for each frame.
  struct Record {
    std::vector<std::pair<traffic_direction_t, std::string_view>> frames;
    uint64_t recv_bytes = 0;
    uint64_t send_bytes = 0;
  };

  virtual ~RecordGenerator() = default;
  // Get the next record for the given connection.
  virtual Record Next(int32_t conn_id) = 0;
};

// Base class for any RecordGenerator that generates records with a single Request and a single
// Response.
class SingleReqRespGen : public RecordGenerator {
 public:
  SingleReqRespGen(std::string_view req_bytes, std::string_view resp_bytes)
      : req_bytes_(req_bytes), resp_bytes_(resp_bytes) {}

  Record Next(int32_t conn_id) override;

 protected:
  std::string req_bytes_;
  std::string resp_bytes_;
};

// Simple HTTP record generator that returns the same http req/resp pair for each record. By default
// it returns a Content-Length encoded http resp with the specified resp_body_size.
class HTTP1SingleReqRespGen : public SingleReqRespGen {
 public:
  static inline std::string_view kDefaultHTTPReq =
      "GET / HTTP/1.1\r\nHost: http-server:8080\r\n\r\n";
  static inline std::string_view kDefaultHTTPRespFmt =
      "HTTP/1.1 200 OK\r\nDate: Fri, 21 Jan 2022 02:31:07 "
      "GMT\r\nContent-Type: text/plain; charset=utf-8\r\n$0\r\n$1";

  explicit HTTP1SingleReqRespGen(size_t total_size, size_t chunk_size = 0, char c = 'b');
};

class MySQLExecuteReqRespGen : public SingleReqRespGen {
 public:
  explicit MySQLExecuteReqRespGen(size_t total_size);
};

class PostgresSelectReqRespGen : public SingleReqRespGen {
 public:
  explicit PostgresSelectReqRespGen(size_t total_size);
};

class CQLQueryReqRespGen : public SingleReqRespGen {
 public:
  explicit CQLQueryReqRespGen(size_t total_size);
};

class NATSMSGGen : public RecordGenerator {
 public:
  explicit NATSMSGGen(size_t total_size, char body_char = 'm');
  Record Next(int32_t conn_id) override;

 private:
  std::string msg_;
};

// Base class for all pos generators. Pos generators generate a sequence of byte positions, based on
// the sequence of message sizes. This allows for interesting distributions of pos, eg. big gaps or
// out of order messages. This is to emulate the byte position bpf adds to each event.
class PosGenerator {
 public:
  virtual ~PosGenerator() = default;
  // Get the next pos given the next message size.
  virtual uint64_t NextPos(uint64_t msg_size) = 0;
  // Notify the PosGenerator that a polling iteration (poll buffers + TransferData) has finished and
  // should update its state accordingly.
  virtual void NextPollIteration() {}
};

// Simple PosGenerator that creates a completely in order stream of positions.
class NoGapsPosGenerator : public PosGenerator {
 public:
  uint64_t NextPos(uint64_t msg_size) override;

 private:
  uint64_t pos_ = 0;
};

// GapPosGenerator creates a stream of byte positions that has gaps of size `gap_size` every
// `max_segment_size`.
class GapPosGenerator : public PosGenerator {
 public:
  GapPosGenerator(uint64_t max_segment_size, uint64_t gap_size)
      : max_segment_size_(max_segment_size), gap_size_(gap_size) {}

  uint64_t NextPos(uint64_t msg_size) override;

 private:
  const uint64_t max_segment_size_;
  const uint64_t gap_size_;
  uint64_t pos_ = 0;
  uint64_t curr_segment_ = 0;
};

// IterationGapPosGenerator creates a stream of byte positions such that for each polling iteration
// (ie. an emulated equivalent of a call to PollPerfBuffers()), the byte positions are completely in
// order, but between polling iterations it adds gaps.
class IterationGapPosGenerator : public PosGenerator {
 public:
  explicit IterationGapPosGenerator(uint64_t gap_size) : gap_size_(gap_size) {}

  uint64_t NextPos(uint64_t msg_size) override;

  void NextPollIteration() override;

 private:
  const uint64_t gap_size_;
  uint64_t pos_ = 0;
};

}  // namespace testing
}  // namespace stirling
}  // namespace px
