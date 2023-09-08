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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <sole.hpp>

#include "src/common/base/base.h"
#include "src/common/base/inet_utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/cql/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

// See section 3 of
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
// for a discussion on types.

// Some complex CQL types defined in the spec.
using StringList = std::vector<std::string>;
using StringMap = std::map<std::string, std::string>;
using StringMultiMap = std::map<std::string, StringList>;

// See section 4.2.5.2 of
// https://git-wip-us.apache.org/repos/asf?p=cassandra.git;a=blob_plain;f=doc/native_protocol_v3.spec
enum class DataType : uint16_t {
  kCustom = 0x0000,
  kAscii = 0x0001,
  kBigint = 0x0002,
  kBlob = 0x0003,
  kBoolean = 0x0004,
  kCounter = 0x0005,
  kDecimal = 0x0006,
  kDouble = 0x0007,
  kFloat = 0x0008,
  kInt = 0x0009,
  kTimestamp = 0x000B,
  kUuid = 0x000C,
  kVarchar = 0x000D,
  kVarint = 0x000E,
  kTimeuuid = 0x000F,
  kInet = 0x0010,
  kList = 0x0020,
  kMap = 0x0021,
  kSet = 0x0022,
  kUDT = 0x0030,
  kTuple = 0x0031,
};

struct Option {
  DataType type;

  // Value is only used if DataType is kCustom.
  std::string value;

  // TODO(oazizi): Store the additional information if DataType is kList/kMap/kSet/kUDT/kTuple.
};

// TODO(oazizi): Consider using std::optional when values are optional in the structs below.

struct NameValuePair {
  std::string name;
  std::basic_string<uint8_t> value;
};

// QueryParameters is a complex type used in QUERY and EXECUTE requests.
// <query_parameters> is composed of:
// <consistency><flags>[<n>[name_1]<value_1>...[name_n]<value_n>]
// [<result_page_size>][<paging_state>][<serial_consistency>][<timestamp>]
// See section 4.1.4 of the spec for more details.
struct QueryParameters {
  uint16_t consistency;
  uint16_t flags;
  std::vector<NameValuePair> values;
  int32_t page_size = 0;
  std::basic_string<uint8_t> paging_state;
  uint16_t serial_consistency = 0;
  int64_t timestamp = 0;
};

// <col_spec> is composed_of:
// (<ksname><tablename>)?<name><type>
// See section 4.2.5.2 of the spec for more details.
struct ColSpec {
  std::string ks_name;
  std::string table_name;
  std::string name;
  Option type;
};

// <metadata> is composed of:
// <flags><columns_count>[<paging_state>][<global_table_spec>?<col_spec_1>...<col_spec_n>]
// See section 4.2.5.2 of the spec for more details.
struct ResultMetadata {
  int32_t flags;
  int32_t columns_count;
  std::basic_string<uint8_t> paging_state;
  std::string gts_keyspace_name;
  std::string gts_table_name;
  std::vector<ColSpec> col_specs;
};

// TODO(oazizi): Consider switching 'change_type' and 'target' strings into enums.
// See section 4.2.6 of the spec for details.
struct SchemaChange {
  // One of "CREATED", "UPDATED" or "DROPPED"
  std::string change_type;

  // One of "KEYSPACE", "TABLE", "TYPE", "FUNCTION" or "AGGREGATE"
  std::string target;

  std::string keyspace;

  // If target is KEYSPACE, then name is unused;
  // If target is TABLE, then name is table name.
  // If target is TYPE, then name is user type name.
  // If target is FUNCTION, then name is function name.
  // If target is AGGREGATE, then name is aggregate name.
  std::string name;

  // Only used for FUNCTION or AGGREGATE.
  StringList arg_types;
};

enum class BatchQueryKind : uint8_t { kString = 0, kID = 1 };

struct BatchQuery {
  BatchQueryKind kind;
  std::variant<std::string, std::basic_string<uint8_t>> query_or_id;
  std::vector<NameValuePair> values;
};

struct ErrorResp {
  int32_t error_code;
  std::string error_msg;
};

struct StartupReq {
  StringMap options;
};

struct ReadyResp {
  // No additional fields.
};

struct AuthenticateResp {
  std::string authenticator_name;
};

struct OptionsReq {
  // No additional fields.
};

struct SupportedResp {
  StringMultiMap options;
};

struct QueryReq {
  std::string query;
  QueryParameters qp;
};

struct ResultVoidResp {};

struct ResultRowsResp {
  ResultMetadata metadata;
  int32_t rows_count;
  // Don't actually grab the row content for now.
};

struct ResultSetKeyspaceResp {
  std::string keyspace_name;
};

struct ResultPreparedResp {
  std::basic_string<uint8_t> id;
  // Note that two metadata are sent back. The first communicates the col specs for the Prepared
  // statement, while the second communicates the metadata for future EXECUTE statements.
  ResultMetadata metadata;
  ResultMetadata result_metadata;
};

struct ResultSchemaChangeResp {
  SchemaChange sc;
};

enum class ResultRespKind : int32_t {
  kVoid = 0x0001,
  kRows = 0x0002,
  kSetKeyspace = 0x0003,
  kPrepared = 0x0004,
  kSchemaChange = 0x0005
};

struct ResultResp {
  ResultRespKind kind;
  std::variant<ResultVoidResp, ResultRowsResp, ResultSetKeyspaceResp, ResultPreparedResp,
               ResultSchemaChangeResp>
      resp;
};

struct PrepareReq {
  std::string query;
};

struct ExecuteReq {
  std::basic_string<uint8_t> id;
  QueryParameters qp;
};

struct RegisterReq {
  StringList event_types;
};

// TODO(oazizi): Consider switching event_type string into enum for efficiency.
struct EventResp {
  std::string event_type;

  // Following fields are for (event_type == "TOPOLOGY_CHANGE" || event_type == "STATUS_CHANGE")
  std::string change_type;
  SockAddr addr;

  // Following fields are for (event_type == "SCHEMA_CHANGE")
  SchemaChange sc;
};

enum class BatchReqType : uint8_t {
  // The batch will be "logged". This is equivalent to a normal CQL3 batch statement.
  kLogged = 0,

  // The batch will be "unlogged".
  kUnlogged = 1,

  // the batch will be a "counter" batch (and non-counter statements will be rejected).
  kCounter = 2,
};

struct BatchReq {
  BatchReqType type;
  std::vector<BatchQuery> queries;
  uint16_t consistency;
  uint8_t flags;
  uint16_t serial_consistency;
  int64_t timestamp;
};

struct AuthChallengeResp {
  std::basic_string<uint8_t> token;
};

struct AuthResponseReq {
  std::basic_string<uint8_t> token;
};

struct AuthSuccessResp {
  std::basic_string<uint8_t> token;
};

/**
 * FrameBodyDecoder provides a structured interface to process the bytes of a CQL frame body.
 *
 * After creating the decoder, successive calls to the Extract<Type> functions will process
 * the bytes as the desired type.
 *
 * If there are not enough bytes to process the type, an error Status will be returned.
 * The decoder will then be in an undefined state, and the result of any subsequent calls
 * to any Extract functions are also undefined.
 */
class FrameBodyDecoder {
 public:
  /**
   * Create a frame decoder.
   *
   * @param buf A string_view into the body of the CQL frame.
   */
  explicit FrameBodyDecoder(std::string_view buf, uint8_t version = 3)
      : binary_decoder_(buf), version_(version) {
    // Actual enforcement happens in cql_parse, so we just CHECK here.
    CTX_DCHECK_GE(version, kMinSupportedProtocolVersion);
    CTX_DCHECK_LE(version, kMaxSupportedProtocolVersion);
  }

  explicit FrameBodyDecoder(const Frame& frame) : FrameBodyDecoder(frame.msg, frame.hdr.version) {}

  // [int] A 4 bytes signed integer.
  StatusOr<int32_t> ExtractInt();

  // [long] A 8 bytes signed integer.
  StatusOr<int64_t> ExtractLong();

  // [short] A 2 bytes unsigned integer.
  StatusOr<uint16_t> ExtractShort();

  // [byte]
  StatusOr<uint8_t> ExtractByte();

  // [string] A [short] n, followed by n bytes representing an UTF-8 string.
  StatusOr<std::string> ExtractString();

  // [long string] An [int] n, followed by n bytes representing an UTF-8 string.
  StatusOr<std::string> ExtractLongString();

  // [uuid] A 16 bytes long uuid.
  StatusOr<sole::uuid> ExtractUUID();

  // [string list] A [short] n, followed by n [string].
  StatusOr<StringList> ExtractStringList();

  // [bytes] A [int] n, followed by n bytes if n >= 0. If n < 0,
  //         no byte should follow and the value represented is `null`.
  StatusOr<std::basic_string<uint8_t>> ExtractBytes();

  // [value] A [int] n, followed by n bytes if n >= 0.
  //         If n == -1 no byte should follow and the value represented is `null`.
  //         If n == -2 no byte should follow and the value represented is
  //         `not set` not resulting in any change to the existing value.
  StatusOr<std::basic_string<uint8_t>> ExtractValue();

  // [short bytes]  A [short] n, followed by n bytes if n >= 0.
  StatusOr<std::basic_string<uint8_t>> ExtractShortBytes();

  // [option] A pair of <id><value> where <id> is a [short] representing
  //          the option id and <value> depends on that option (and can be
  //          of size 0). The supported id (and the corresponding <value>)
  //          will be described when this is used.
  StatusOr<Option> ExtractOption();

  // [inet] An address (ip and port) to a node. It consists of one
  //        [byte] n, that represents the address size, followed by n
  //        [byte] representing the IP address (in practice n can only be
  //        either 4 (IPv4) or 16 (IPv6)), following by one [int]
  //        representing the port.
  StatusOr<SockAddr> ExtractInet();

  // [string map] A [short] n, followed by n pair <k><v> where <k> and <v>
  //              are [string].
  StatusOr<StringMap> ExtractStringMap();

  // [string multimap] A [short] n, followed by n pair <k><v> where <k> is a
  //                   [string] and <v> is a [string list].
  StatusOr<StringMultiMap> ExtractStringMultiMap();

  // Extracts a name-value pair.
  // Name may not be present; with_names specifies whether name should be present or not.
  // When with_names == false, name will be left empty.
  StatusOr<NameValuePair> ExtractNameValuePair(bool with_names);

  // Extracts a list of name-value pairs.
  StatusOr<std::vector<NameValuePair>> ExtractNameValuePairList(bool with_names);

  // Extracts query parameters, which is a complex type. See struct for details.
  StatusOr<QueryParameters> ExtractQueryParameters();

  // Extracts result metadata, which is a complex type. See struct for details.
  // @param There are two variants of result metadata. If the metadata is part of a result
  // with kind=prepared, then set prepared_result_metadata to true, so it parses correctly.
  StatusOr<ResultMetadata> ExtractResultMetadata(bool prepared_result_metadata = false);

  // Extracts a schema change response. See struct for details.
  StatusOr<SchemaChange> ExtractSchemaChange();

  /**
   * Whether processing has reached end-of-frame.
   */
  bool eof() { return binary_decoder_.eof(); }

  Status ExpectEOF() {
    if (!eof()) {
      return error::Internal("There are still $0 bytes left", binary_decoder_.BufSize());
    }
    return Status::OK();
  }

 private:
  template <typename TIntType>
  StatusOr<TIntType> ExtractIntCore();

  template <typename TCharType>
  StatusOr<std::basic_string<TCharType>> ExtractBytesCore(int64_t len);

  template <typename TCharType, size_t N>
  Status ExtractBytesCore(TCharType* out);

  BinaryDecoder binary_decoder_;

  // Version of the CQL binary protocol to use when decoding.
  const uint8_t version_;
};

StatusOr<ErrorResp> ParseErrorResp(Frame* frame);
StatusOr<StartupReq> ParseStartupReq(Frame* frame);
StatusOr<ReadyResp> ParseReadyResp(Frame* frame);
StatusOr<AuthenticateResp> ParseAuthenticateResp(Frame* frame);
StatusOr<OptionsReq> ParseOptionsReq(Frame* frame);
StatusOr<SupportedResp> ParseSupportedResp(Frame* frame);
StatusOr<QueryReq> ParseQueryReq(Frame* frame);
StatusOr<ResultResp> ParseResultResp(Frame* frame);
StatusOr<PrepareReq> ParsePrepareReq(Frame* frame);
StatusOr<ExecuteReq> ParseExecuteReq(Frame* frame);
StatusOr<RegisterReq> ParseRegisterReq(Frame* frame);
StatusOr<EventResp> ParseEventResp(Frame* frame);
StatusOr<BatchReq> ParseBatchReq(Frame* frame);
StatusOr<AuthChallengeResp> ParseAuthChallengeResp(Frame* frame);
StatusOr<AuthResponseReq> ParseAuthResponseReq(Frame* frame);
StatusOr<AuthSuccessResp> ParseAuthSuccessResp(Frame* frame);

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
