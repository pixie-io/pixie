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
#include <stack>
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
  kBoolean,
  kInt8,
  kInt16,
  kInt32,
  kInt64,
  kUint32,
  kVarint,
  kVarlong,
  kUuid,
  kFloat64,
  kString,
  kCompactString,
  kNullableString,
  kCompactNullableString,
  kBytes,
  kCompactBytes,
  kNullableBytes,
  kCompactNullableBytes,
  kRecords,
  kArray,
  kCompactArray,
};

struct RecordMessage {
  std::string key;
  std::string value;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("key");
    writer->String(key.c_str());
    writer->Key("value");
    writer->String(value.c_str());
    writer->EndObject();
  }
};

struct RecordBatch {
  std::vector<RecordMessage> records;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("records");
    writer->StartArray();
    for (const auto& r : records) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct ProduceReqPartition {
  int32_t index = 0;
  RecordBatch record_batch;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("record_batch");
    record_batch.ToJSON(writer);
    writer->EndObject();
  }
};

struct ProduceReqTopic {
  std::string name;
  std::vector<ProduceReqPartition> partitions;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("name");
    writer->String(name.c_str());
    writer->Key("partitions");
    writer->StartArray();
    for (const auto& r : partitions) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

// Produce Request Message (opcode = 0).
struct ProduceReq {
  std::string transactional_id;
  int16_t acks = 0;
  int32_t timeout_ms = 0;
  std::vector<ProduceReqTopic> topics;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("transactional_id");
    writer->String(transactional_id.c_str());
    writer->Key("acks");
    writer->Int(acks);
    writer->Key("timeout_ms");
    writer->Int(timeout_ms);
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct RecordError {
  int32_t batch_index;
  std::string error_message;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("batch_index");
    writer->Int(batch_index);
    writer->Key("error_message");
    writer->String(error_message.c_str());
    writer->EndObject();
  }
};

struct ProduceRespPartition {
  int32_t index;
  int16_t error_code;
  std::vector<RecordError> record_errors;
  std::string error_message;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("index");
    writer->Int(index);
    writer->Key("error_code");
    writer->String(magic_enum::enum_name(static_cast<ErrorCode>(error_code)).data());
    writer->Key("record_errors");
    writer->StartArray();
    for (const auto& r : record_errors) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("error_message");
    writer->String(error_message.c_str());
    writer->EndObject();
  }
};

struct ProduceRespTopic {
  std::string name;
  std::vector<ProduceRespPartition> partitions;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("name");
    writer->String(name.c_str());
    writer->Key("partitions");
    writer->StartArray();
    for (const auto& r : partitions) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->EndObject();
  }
};

struct ProduceResp {
  std::vector<ProduceRespTopic> topics;
  int32_t throttle_time_ms;

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("topics");
    writer->StartArray();
    for (const auto& r : topics) {
      r.ToJSON(writer);
    }
    writer->EndArray();
    writer->Key("throttle_time_ms");
    writer->Int(throttle_time_ms);
    writer->EndObject();
  }
};

template <typename T>
std::string ToString(T obj) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  obj.ToJSON(&writer);
  return sb.GetString();
}

class PacketDecoder {
 public:
  explicit PacketDecoder(std::string_view buf) : marked_bufs_(), binary_decoder_(buf) {}
  explicit PacketDecoder(const Packet& packet) : PacketDecoder(packet.msg) {}

  StatusOr<int8_t> ExtractInt8();

  StatusOr<int16_t> ExtractInt16();

  StatusOr<int32_t> ExtractInt32();

  StatusOr<int64_t> ExtractInt64();

  StatusOr<int32_t> ExtractUnsignedVarint();

  // Represents an integer between -2^31 and 2^31-1 inclusive. Encoding follows the
  // variable-length zig-zag encoding from Google Protocol Buffers.
  // https://developers.google.com/protocol-buffers/docs/encoding#varints
  StatusOr<int32_t> ExtractVarint();

  // Represents an integer between -2^63 and 2^63-1 inclusive. Encoding follows the variable-length
  // zig-zag encoding from Google Protocol Buffers.
  // https://developers.google.com/protocol-buffers/docs/encoding#varints
  StatusOr<int64_t> ExtractVarlong();

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

  // Represents bytes whose length is encoded with zigzag varint.
  StatusOr<std::string> ExtractBytesZigZag();

  // TODO(chengruizhe): Use std::function in ExtractArray and ExtractCompactArray.
  // Represents a sequence of objects of a given type T. Type T can be either a primitive
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
    for (int i = 0; i < len; ++i) {
      PL_ASSIGN_OR_RETURN(T tmp, (this->*extract_func)());
      result.push_back(std::move(tmp));
    }
    return result;
  }

  // Represents a sequence of objects of a given type T. Type T can be either a
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
    for (int i = 0; i < len; ++i) {
      PL_ASSIGN_OR_RETURN(T tmp, (this->*extract_func)());
      result.push_back(std::move(tmp));
    }
    return result;
  }

  // TODO(chengruizhe): Parse and return a TagSection struct if needed.
  // In a flexible version, each structure ends with a tag section.
  // For more info:
  // https://cwiki.apache.org/confluence/display/KAFKA/KIP-482%3A+The+Kafka+Protocol+should+Support+Optional+Tagged+Fields#KIP482:TheKafkaProtocolshouldSupportOptionalTaggedFields-FlexibleVersions
  Status ExtractTagSection();

  Status ExtractTaggedField();

  // Messages consist of a variable-length header, a variable-length opaque key byte array and a
  // variable-length opaque value byte array.
  // https://kafka.apache.org/documentation/#record
  StatusOr<RecordMessage> ExtractRecordMessage();

  // Messages (aka Records) are always written in batches. The technical term for a batch of
  // messages is a record batch, and a record batch contains one or more records.
  // https://kafka.apache.org/documentation/#recordbatch
  StatusOr<RecordBatch> ExtractRecordBatch();

  // Partition Data in Produce Request.
  StatusOr<ProduceReqPartition> ExtractProduceReqPartition();

  // Topic Data in Produce Request.
  StatusOr<ProduceReqTopic> ExtractProduceReqTopic();

  // RecordError field in Produce Response with api_version >= 8.
  StatusOr<RecordError> ExtractRecordError();

  // Partition Data in Produce Response.
  StatusOr<ProduceRespPartition> ExtractProduceRespPartition();

  // Topic Data in Produce Request.
  StatusOr<ProduceRespTopic> ExtractProduceRespTopic();

  Status ExtractReqHeader(Request* req);
  Status ExtractRespHeader(Response* resp);

  StatusOr<ProduceReq> ExtractProduceReq();
  StatusOr<ProduceResp> ExtractProduceResp();

  bool eof() { return binary_decoder_.eof(); }

  void SetAPIInfo(APIKey api_key, int16_t api_version) {
    api_version_ = api_version;
    is_flexible_ = IsFlexible(api_key, api_version);
  }

 private:
  template <typename TCharType>
  StatusOr<std::basic_string<TCharType>> ExtractBytesCore(int32_t len);

  template <uint8_t TMaxLength>
  StatusOr<int64_t> ExtractUnsignedVarintCore();

  template <uint8_t TMaxLength>
  StatusOr<int64_t> ExtractVarintCore();

  // Sometimes it's more efficient to parse out some fields and jump to an offset indicated
  // by a length. This also makes parsing more robust, as it sets boundaries. MarkOffset and
  // JumpToOffset should be used in pairs.
  Status MarkOffset(int32_t len) {
    DCHECK_GE(len, 0);
    if ((size_t)len > binary_decoder_.Buf().size()) {
      return error::Internal("Not enough bytes in MarkOffset.");
    }
    marked_bufs_.push(binary_decoder_.Buf().substr(len));
    return Status::OK();
  }

  Status JumpToOffset() {
    DCHECK(!marked_bufs_.empty());
    binary_decoder_.SetBuf(marked_bufs_.top());
    marked_bufs_.pop();
    return Status::OK();
  }

  std::stack<std::string_view> marked_bufs_;
  BinaryDecoder binary_decoder_;
  int16_t api_version_ = 0;
  bool is_flexible_ = false;
};

}  // namespace kafka
}  // namespace protocols
}  // namespace stirling
}  // namespace px
