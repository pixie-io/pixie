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

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/kafka/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace kafka {

enum class DataType : uint16_t {
  kBoolean = 0,
  kInt8 = 1,
  kInt16 = 2,
  kInt32 = 3,
  kInt64 = 4,
  kUint32 = 5,
  kVarint = 6,
  kVarlong = 7,
  kUuid = 8,
  kFloat64 = 9,
  kString = 10,
  kCompactString = 11,
  kNullableString = 12,
  kCompactNullableString = 13,
  kBytes = 14,
  kCompactBytes = 15,
  kNullableBytes = 16,
  kCompactNullableBytes = 17,
  kRecords = 18,
  kArray = 19,
  kCompactArray = 20,
};

// TODO(chengruizhe): Support the complete ProduceReq.
// Produce Request Message (opcode = 0).
struct ProduceReq {
  std::string transactional_id;
  int16_t acks = 0;
  int32_t timeout_ms = 0;
  int32_t num_topics = 0;

  std::string ToJSONString() const {
    std::map<std::string, std::string> fields = {
        {"transactional_id", transactional_id},
        {"acks", std::to_string(acks)},
        {"timeout_ms", std::to_string(timeout_ms)},
        {"num_topics", std::to_string(num_topics)},
    };
    return utils::ToJSONString(fields);
  }
};

// TODO(chengruizhe): Support the complete ProduceResp.
struct ProduceResp {
  int32_t num_responses = 0;

  std::string ToJSONString() const {
    std::map<std::string, std::string> fields = {
        {"num_responses", std::to_string(num_responses)},
    };
    return utils::ToJSONString(fields);
  }
};

class PacketDecoder {
 public:
  explicit PacketDecoder(std::string_view buf) : binary_decoder_(buf) {}
  explicit PacketDecoder(const Packet& packet) : PacketDecoder(packet.msg) {}

  StatusOr<int8_t> ExtractInt8();

  StatusOr<int16_t> ExtractInt16();

  StatusOr<int32_t> ExtractInt32();

  StatusOr<int64_t> ExtractInt64();

  StatusOr<int32_t> ExtractUnsignedVarint();

  // Represents an integer between -231 and 231-1 inclusive. Encoding follows the
  // variable-length zig-zag encoding from Google Protocol Buffers.
  // https://developers.google.com/protocol-buffers/docs/encoding#varints
  StatusOr<int32_t> ExtractVarint();

  // Represents a sequence of characters. First the length N is given as an INT16. Then N
  // bytes follow which are the UTF-8 encoding of the character sequence.
  StatusOr<std::string> ExtractString();

  // Represents a sequence of characters or null. For non-null strings, first the
  // length N is given as an INT16. Then N bytes follow which are the UTF-8 encoding of the
  // character sequence. A null value is encoded with length of -1 and there are no following
  // bytes.
  StatusOr<std::string> ExtractNullableString();

  // Represents a sequence of characters. First the length N + 1 is given as an
  // UNSIGNED_VARINT . Then N bytes follow which are the UTF-8 encoding of the character sequence.
  StatusOr<std::string> ExtractCompactString();

  // Represents a sequence of characters. First the length N + 1 is given
  // as an UNSIGNED_VARINT . Then N bytes follow which are the UTF-8 encoding of the character
  // sequence. A null string is represented with a length of 0.
  StatusOr<std::string> ExtractCompactNullableString();

  // ARRAY. Represents a sequence of objects of a given type T. Type T can be either a primitive
  // type (e.g. STRING) or a structure. First, the length N is given as an INT32. Then N instances
  // of type T follow. A null array is represented with a length of -1.
  template <typename T>
  StatusOr<std::vector<T>> ExtractArray(StatusOr<T> (PacketDecoder::*extract_func)()) {
    constexpr int kNullSize = -1;

    PL_ASSIGN_OR_RETURN(int32_t len, ExtractInt32());
    if (len < kNullSize) {
      return error::Internal("Length of array cannot be negative.");
    }
    if (len == kNullSize) {
      return std::vector<T>();
    }

    std::vector<T> result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
      PL_ASSIGN_OR_RETURN(T tmp, (this->*extract_func)());
      result.push_back(std::move(tmp));
    }
    return result;
  }

  // COMPACT ARRAY. Represents a sequence of objects of a given type T. Type T can be either a
  // primitive type (e.g. STRING) or a structure. First, the length N + 1 is given as an
  // UNSIGNED_VARINT. Then N instances of type T follow. A null array is represented with a length
  // of 0.
  template <typename T>
  StatusOr<std::vector<T>> ExtractCompactArray(StatusOr<T> (PacketDecoder::*extract_func)()) {
    PL_ASSIGN_OR_RETURN(int32_t len, ExtractUnsignedVarint());
    if (len < 0) {
      return error::Internal("Length of array cannot be negative.");
    }
    if (len == 0) {
      return std::vector<T>();
    }
    // Length N + 1 is encoded.
    len -= 1;

    std::vector<T> result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
      PL_ASSIGN_OR_RETURN(T tmp, (this->*extract_func)());
      result.push_back(std::move(tmp));
    }
    return result;
  }

  Status ExtractReqHeader(Request* req);
  Status ExtractRespHeader(Response* resp);

  StatusOr<ProduceReq> ExtractProduceReq();
  StatusOr<ProduceResp> ExtractProduceResp();

  bool eof() { return binary_decoder_.eof(); }

  void set_api_version(int16_t api_version) { api_version_ = api_version; }

 private:
  template <typename TCharType>
  StatusOr<std::basic_string<TCharType>> ExtractBytesCore(int16_t len);

  BinaryDecoder binary_decoder_;
  int16_t api_version_ = 0;
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
