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

#include "src/stirling/source_connectors/socket_tracer/protocols/cql/frame_body_decoder.h"

#include <algorithm>
#include <utility>

#include "src/common/base/byte_utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace cass {

// These Extract functions parse raw byte sequences into CQL types.
// Note that the CQL protocol is big-endian, so all extract functions
// also intrinsically convert from big-endian to host ordering.

template <typename TIntType>
StatusOr<TIntType> FrameBodyDecoder::ExtractIntCore() {
  return binary_decoder_.ExtractBEInt<TIntType>();
}

template <typename TFloatType>
StatusOr<TFloatType> ExtractFloatCore(std::string_view* buf) {
  if (buf->size() < sizeof(TFloatType)) {
    return error::ResourceUnavailable("Insufficient number of bytes.");
  }
  TFloatType val = utils::BEndianBytesToFloat<TFloatType>(*buf);
  buf->remove_prefix(sizeof(TFloatType));
  return val;
}

template <typename TCharType>
StatusOr<std::basic_string<TCharType>> FrameBodyDecoder::ExtractBytesCore(int64_t len) {
  PX_ASSIGN_OR_RETURN(std::basic_string_view<TCharType> tbuf,
                      binary_decoder_.ExtractString<TCharType>(len));
  return std::basic_string<TCharType>(tbuf);
}

template <typename TCharType, size_t N>
Status FrameBodyDecoder::ExtractBytesCore(TCharType* out) {
  PX_ASSIGN_OR_RETURN(std::basic_string_view<TCharType> tbuf,
                      binary_decoder_.ExtractString<TCharType>(N));
  memcpy(out, tbuf.data(), N);
  return Status::OK();
}

// [int] A 4 bytes signed integer
StatusOr<int32_t> FrameBodyDecoder::ExtractInt() { return ExtractIntCore<int32_t>(); }

// [long] A 8 bytes signed integer
StatusOr<int64_t> FrameBodyDecoder::ExtractLong() { return ExtractIntCore<int64_t>(); }

// [short] A 2 bytes unsigned integer
StatusOr<uint16_t> FrameBodyDecoder::ExtractShort() { return ExtractIntCore<uint16_t>(); }

// [byte] A 2 bytes unsigned integer
StatusOr<uint8_t> FrameBodyDecoder::ExtractByte() { return ExtractIntCore<uint8_t>(); }

// [float]
StatusOr<float> ExtractFloat(std::string_view* buf) { return ExtractFloatCore<float>(buf); }

// [double]
StatusOr<double> ExtractDouble(std::string_view* buf) { return ExtractFloatCore<double>(buf); }

// [string] A [short] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> FrameBodyDecoder::ExtractString() {
  PX_ASSIGN_OR_RETURN(uint16_t len, ExtractShort());
  return ExtractBytesCore<char>(len);
}

// [long string] An [int] n, followed by n bytes representing an UTF-8 string.
StatusOr<std::string> FrameBodyDecoder::ExtractLongString() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractInt());
  len = std::max(len, 0);
  return ExtractBytesCore<char>(len);
}

// [uuid] A 16 bytes long uuid.
StatusOr<sole::uuid> FrameBodyDecoder::ExtractUUID() {
  sole::uuid uuid;

  // Logically, we want to get the different components of the UUID, and ensure correct byte-order.
  // For example, see datastax:
  // https://github.com/datastax/cpp-driver/blob/bbbbd7bc3eaba1b10ad8ac6f53c41fa93ee718db/src/serialization.hpp
  // They do it in components, because each component is big-endian ordered.
  // The ordering of bytes for the entire UUID is effectively:
  //   input:  {15 ...........  8  7  6  5  4  3  2  1  0}
  //   output: {8 ............ 15}{6  7}{4  5}{0  1  2  3}
  //
  // Equivalent code would be:
  //   PX_ASSIGN_OR_RETURN(uint64_t time_low, ExtractInt(buf));
  //   PX_ASSIGN_OR_RETURN(uint64_t time_mid, ExtractShort(buf));
  //   PX_ASSIGN_OR_RETURN(uint64_t time_hi_version, ExtractShort(buf));
  //   PX_ASSIGN_OR_RETURN(uint64_t clock_seq_and_node, ExtractLong(buf));
  //
  // But then we constitute the components according to the following formula,
  // from uuid1() in sole.hpp:
  //
  //   uuid.ab = (time_low << 32) | (time_mid << 16) | time_hi_version;
  //   uuid.cd = clock_seq_and_node;
  //
  // But we notice that the outcome of all this is:
  //   uuid.ab = {0  1  2  3}{4  5}{6  7}
  //   uuid.cd = {8 ................. 15}
  //
  // And we realize that we can achieve this directly with the following shortcut:

  PX_ASSIGN_OR_RETURN(uuid.ab, ExtractLong());
  PX_ASSIGN_OR_RETURN(uuid.cd, ExtractLong());

  return uuid;
}

// [string list] A [short] n, followed by n [string].
StatusOr<StringList> FrameBodyDecoder::ExtractStringList() {
  PX_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringList string_list;
  for (int i = 0; i < n; ++i) {
    PX_ASSIGN_OR_RETURN(std::string s, ExtractString());
    string_list.push_back(std::move(s));
  }

  return string_list;
}

// [bytes] A [int] n, followed by n bytes if n >= 0. If n < 0,
//         no byte should follow and the value represented is `null`.
StatusOr<std::basic_string<uint8_t>> FrameBodyDecoder::ExtractBytes() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractInt());
  len = std::max(len, 0);
  return ExtractBytesCore<uint8_t>(len);
}

// A [int] n, followed by n bytes if n >= 0.
//         If n == -1 no byte should follow and the value represented is `null`.
//         If n == -2 no byte should follow and the value represented is
//         `not set` not resulting in any change to the existing value.
StatusOr<std::basic_string<uint8_t>> FrameBodyDecoder::ExtractValue() {
  PX_ASSIGN_OR_RETURN(int32_t len, ExtractInt());
  if (len == -1) {
    return std::basic_string<uint8_t>();
  }
  if (len == -2) {
    // TODO(oazizi): Need to send back 'not set' instead.
    return std::basic_string<uint8_t>();
  }
  if (len < 0) {
    return error::Internal("Invalid length for value.");
  }
  return ExtractBytesCore<uint8_t>(len);
}

// [short bytes]  A [short] n, followed by n bytes if n >= 0.
StatusOr<std::basic_string<uint8_t>> FrameBodyDecoder::ExtractShortBytes() {
  PX_ASSIGN_OR_RETURN(uint16_t len, ExtractShort());
  return ExtractBytesCore<uint8_t>(len);
}

// [inet] An address (ip and port) to a node. It consists of one
//        [byte] n, that represents the address size, followed by n
//        [byte] representing the IP address (in practice n can only be
//        either 4 (IPv4) or 16 (IPv6)), following by one [int]
//        representing the port.
StatusOr<SockAddr> FrameBodyDecoder::ExtractInet() {
  PX_ASSIGN_OR_RETURN(uint8_t n, ExtractByte());

  SockAddr addr;

  switch (n) {
    case 4: {
      addr.family = SockAddrFamily::kIPv4;
      auto& addr4 = addr.addr.emplace<SockAddrIPv4>();
      PX_RETURN_IF_ERROR((ExtractBytesCore<uint8_t, 4>(reinterpret_cast<uint8_t*>(&addr4.addr))));
      PX_ASSIGN_OR_RETURN(addr4.port, ExtractInt());
    } break;
    case 16: {
      addr.family = SockAddrFamily::kIPv6;
      auto& addr6 = addr.addr.emplace<SockAddrIPv6>();
      PX_RETURN_IF_ERROR((ExtractBytesCore<uint8_t, 16>(reinterpret_cast<uint8_t*>(&addr6.addr))));
      PX_ASSIGN_OR_RETURN(addr6.port, ExtractInt());
    } break;
  }

  return addr;
}

// [string map] A [short] n, followed by n pair <k><v> where <k> and <v>
//              are [string].
StatusOr<StringMap> FrameBodyDecoder::ExtractStringMap() {
  PX_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringMap string_map;
  for (int i = 0; i < n; ++i) {
    PX_ASSIGN_OR_RETURN(std::string key, ExtractString());
    PX_ASSIGN_OR_RETURN(std::string val, ExtractString());
    string_map.insert({std::move(key), std::move(val)});
  }

  return string_map;
}

// [string multimap] A [short] n, followed by n pair <k><v> where <k> is a
//                   [string] and <v> is a [string list].
StatusOr<StringMultiMap> FrameBodyDecoder::ExtractStringMultiMap() {
  PX_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());

  StringMultiMap string_multimap;
  for (int i = 0; i < n; ++i) {
    PX_ASSIGN_OR_RETURN(std::string key, ExtractString());
    PX_ASSIGN_OR_RETURN(StringList val, ExtractStringList());
    string_multimap.insert({std::move(key), std::move(val)});
  }

  return string_multimap;
}

StatusOr<Option> FrameBodyDecoder::ExtractOption() {
  Option col_spec;
  PX_ASSIGN_OR_RETURN(uint16_t id, ExtractShort());
  col_spec.type = static_cast<DataType>(id);
  if (col_spec.type == DataType::kCustom) {
    PX_ASSIGN_OR_RETURN(col_spec.value, ExtractString());
  }
  if (col_spec.type == DataType::kList || col_spec.type == DataType::kSet) {
    PX_ASSIGN_OR_RETURN(Option type, ExtractOption());
    // For now, we're throwing the result away. Could consider recording if desired.
  }
  if (col_spec.type == DataType::kMap) {
    PX_ASSIGN_OR_RETURN(Option key_type, ExtractOption());
    PX_ASSIGN_OR_RETURN(Option val_type, ExtractOption());
    // For now, we're throwing the result away. Could consider recording if desired.
  }

  // TODO(oazizi): Process kUDT and kTuple.
  CTX_DCHECK(col_spec.type != DataType::kUDT);
  CTX_DCHECK(col_spec.type != DataType::kTuple);

  return col_spec;
}

StatusOr<NameValuePair> FrameBodyDecoder::ExtractNameValuePair(bool with_names) {
  NameValuePair nv;

  if (with_names) {
    PX_ASSIGN_OR_RETURN(nv.name, ExtractString());
  }
  PX_ASSIGN_OR_RETURN(nv.value, ExtractValue());

  return nv;
}

StatusOr<std::vector<NameValuePair>> FrameBodyDecoder::ExtractNameValuePairList(bool with_names) {
  std::vector<NameValuePair> values;

  PX_ASSIGN_OR_RETURN(uint16_t n, ExtractShort());
  for (int i = 0; i < n; ++i) {
    PX_ASSIGN_OR_RETURN(NameValuePair v, ExtractNameValuePair(with_names));
    values.push_back(std::move(v));
  }

  return values;
}

StatusOr<QueryParameters> FrameBodyDecoder::ExtractQueryParameters() {
  QueryParameters qp;

  PX_ASSIGN_OR_RETURN(qp.consistency, ExtractShort());
  PX_ASSIGN_OR_RETURN(qp.flags, ExtractByte());

  bool flag_values = qp.flags & 0x01;
  bool flag_skip_metadata = qp.flags & 0x02;
  bool flag_page_size = qp.flags & 0x04;
  bool flag_with_paging_state = qp.flags & 0x08;
  bool flag_with_serial_consistency = qp.flags & 0x10;
  bool flag_with_default_timestamp = qp.flags & 0x20;
  bool flag_with_names_for_values = qp.flags & 0x40;
  PX_UNUSED(flag_skip_metadata);

  if (flag_values) {
    PX_ASSIGN_OR_RETURN(qp.values, ExtractNameValuePairList(flag_with_names_for_values));
  }

  if (flag_page_size) {
    PX_ASSIGN_OR_RETURN(qp.page_size, ExtractInt());
  }

  if (flag_with_paging_state) {
    PX_ASSIGN_OR_RETURN(qp.paging_state, ExtractBytes());
  }

  if (flag_with_serial_consistency) {
    PX_ASSIGN_OR_RETURN(qp.serial_consistency, ExtractShort());
  }

  if (flag_with_default_timestamp) {
    PX_ASSIGN_OR_RETURN(qp.timestamp, ExtractLong());
  }

  return qp;
}

StatusOr<ResultMetadata> FrameBodyDecoder::ExtractResultMetadata(bool prepared_result_metadata) {
  ResultMetadata r;
  PX_ASSIGN_OR_RETURN(r.flags, ExtractInt());
  PX_ASSIGN_OR_RETURN(r.columns_count, ExtractInt());

  // Version 4+ of the protocol has partition-key bind indexes
  // when the metadata is in response to a PREPARE request.
  bool has_pk = prepared_result_metadata && (version_ >= 4);
  if (has_pk) {
    PX_ASSIGN_OR_RETURN(int32_t pk_count, ExtractInt());
    for (int i = 0; i < pk_count; ++i) {
      PX_ASSIGN_OR_RETURN(uint16_t pk_index_i, ExtractShort());
      PX_UNUSED(pk_index_i);
    }
  }

  bool flag_global_tables_spec = r.flags & 0x0001;
  bool flag_has_more_pages = r.flags & 0x0002;
  bool flag_no_metadata = r.flags & 0x0004;

  if (flag_has_more_pages) {
    PX_ASSIGN_OR_RETURN(r.paging_state, ExtractBytes());
  }

  if (!flag_no_metadata) {
    if (flag_global_tables_spec) {
      PX_ASSIGN_OR_RETURN(r.gts_keyspace_name, ExtractString());
      PX_ASSIGN_OR_RETURN(r.gts_table_name, ExtractString());
    }

    for (int i = 0; i < r.columns_count; ++i) {
      ColSpec col_spec;
      if (!flag_global_tables_spec) {
        PX_ASSIGN_OR_RETURN(col_spec.ks_name, ExtractString());
        PX_ASSIGN_OR_RETURN(col_spec.table_name, ExtractString());
      }
      PX_ASSIGN_OR_RETURN(col_spec.name, ExtractString());
      PX_ASSIGN_OR_RETURN(col_spec.type, ExtractOption());
      r.col_specs.push_back(std::move(col_spec));
    }
  }

  return r;
}

StatusOr<SchemaChange> FrameBodyDecoder::ExtractSchemaChange() {
  SchemaChange sc;

  PX_ASSIGN_OR_RETURN(sc.change_type, ExtractString());
  PX_ASSIGN_OR_RETURN(sc.target, ExtractString());
  PX_ASSIGN_OR_RETURN(sc.keyspace, ExtractString());

  if (sc.target != "KEYSPACE") {
    // Targets TABLE, TYPE, FUNCTION and AGGREGATE all have a name.
    PX_ASSIGN_OR_RETURN(sc.name, ExtractString());
  }

  if (sc.target == "FUNCTION" || sc.target == "AGGREGATE") {
    // Targets FUNCTION and AGGREGATE also have argument types.
    PX_ASSIGN_OR_RETURN(sc.arg_types, ExtractStringList());
  }

  return sc;
}

StatusOr<StartupReq> ParseStartupReq(Frame* frame) {
  StartupReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.options, decoder.ExtractStringMap());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<AuthResponseReq> ParseAuthResponseReq(Frame* frame) {
  AuthResponseReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.token, decoder.ExtractBytes());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<OptionsReq> ParseOptionsReq(Frame* frame) {
  OptionsReq r;
  FrameBodyDecoder decoder(*frame);
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<RegisterReq> ParseRegisterReq(Frame* frame) {
  RegisterReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.event_types, decoder.ExtractStringList());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<QueryReq> ParseQueryReq(Frame* frame) {
  QueryReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.query, decoder.ExtractLongString());
  PX_ASSIGN_OR_RETURN(r.qp, decoder.ExtractQueryParameters());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<PrepareReq> ParsePrepareReq(Frame* frame) {
  PrepareReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.query, decoder.ExtractLongString());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<ExecuteReq> ParseExecuteReq(Frame* frame) {
  ExecuteReq r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.id, decoder.ExtractShortBytes());
  PX_ASSIGN_OR_RETURN(r.qp, decoder.ExtractQueryParameters());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<BatchReq> ParseBatchReq(Frame* frame) {
  BatchReq r;

  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(uint8_t type_raw, decoder.ExtractByte());
  PX_ASSIGN_OR_RETURN(r.type, EnumCast<BatchReqType>(type_raw));

  PX_ASSIGN_OR_RETURN(uint16_t n, decoder.ExtractShort());

  for (uint i = 0; i < n; ++i) {
    BatchQuery q;
    PX_ASSIGN_OR_RETURN(uint8_t kind_raw, decoder.ExtractByte());
    PX_ASSIGN_OR_RETURN(q.kind, EnumCast<BatchQueryKind>(kind_raw));
    switch (q.kind) {
      case BatchQueryKind::kString: {
        PX_ASSIGN_OR_RETURN(q.query_or_id, decoder.ExtractLongString());
        break;
      }
      case BatchQueryKind::kID: {
        PX_ASSIGN_OR_RETURN(q.query_or_id, decoder.ExtractShortBytes());
        break;
      }
      default:
        // EnumCast should ensure we never get here.
        LOG(DFATAL) << absl::Substitute("Unrecognized BatchQueryKind $0", static_cast<int>(q.kind));
    }

    // See note below about flag_with_names_for_values.
    PX_ASSIGN_OR_RETURN(q.values, decoder.ExtractNameValuePairList(false));
    r.queries.push_back(std::move(q));
  }

  PX_ASSIGN_OR_RETURN(r.consistency, decoder.ExtractShort());
  PX_ASSIGN_OR_RETURN(r.flags, decoder.ExtractByte());

  bool flag_with_serial_consistency = r.flags & 0x10;
  bool flag_with_default_timestamp = r.flags & 0x20;
  bool flag_with_names_for_values = r.flags & 0x40;

  // Note that the flag `with_names_for_values` occurs after its use in the spec,
  // that's why we have hard-coded the value to false in the call to ExtractNameValuePairList()
  // above. This is actually what the spec defines, because of the spec bug:
  //
  // With names for values. If set, then all values for all <query_i> must be
  // preceded by a [string] <name_i> that have the same meaning as in QUERY
  // requests [IMPORTANT NOTE: this feature does not work and should not be
  // used. It is specified in a way that makes it impossible for the server
  // to implement. This will be fixed in a future version of the native
  // protocol. See https://issues.apache.org/jira/browse/CASSANDRA-10246 for
  // more details].
  PX_UNUSED(flag_with_names_for_values);

  if (flag_with_serial_consistency) {
    PX_ASSIGN_OR_RETURN(r.serial_consistency, decoder.ExtractShort());
  }

  if (flag_with_default_timestamp) {
    PX_ASSIGN_OR_RETURN(r.timestamp, decoder.ExtractLong());
  }

  PX_RETURN_IF_ERROR(decoder.ExpectEOF());

  return r;
}

StatusOr<ErrorResp> ParseErrorResp(Frame* frame) {
  ErrorResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.error_code, decoder.ExtractInt());
  PX_ASSIGN_OR_RETURN(r.error_msg, decoder.ExtractString());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<ReadyResp> ParseReadyResp(Frame* frame) {
  ReadyResp r;
  FrameBodyDecoder decoder(*frame);
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<SupportedResp> ParseSupportedResp(Frame* frame) {
  SupportedResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.options, decoder.ExtractStringMultiMap());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<AuthenticateResp> ParseAuthenticateResp(Frame* frame) {
  AuthenticateResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.authenticator_name, decoder.ExtractString());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<AuthSuccessResp> ParseAuthSuccessResp(Frame* frame) {
  AuthSuccessResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.token, decoder.ExtractBytes());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

StatusOr<AuthChallengeResp> ParseAuthChallengeResp(Frame* frame) {
  AuthChallengeResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.token, decoder.ExtractBytes());
  PX_RETURN_IF_ERROR(decoder.ExpectEOF());
  return r;
}

namespace {

StatusOr<ResultVoidResp> ParseResultVoid(FrameBodyDecoder* decoder) {
  ResultVoidResp r;
  PX_RETURN_IF_ERROR(decoder->ExpectEOF());
  return r;
}

// See section 4.2.5.2 of the spec.
StatusOr<ResultRowsResp> ParseResultRows(FrameBodyDecoder* decoder) {
  ResultRowsResp r;
  PX_ASSIGN_OR_RETURN(r.metadata, decoder->ExtractResultMetadata());
  PX_ASSIGN_OR_RETURN(r.rows_count, decoder->ExtractInt());
  // Skip grabbing the row content for now.
  // PX_RETURN_IF_ERROR(decoder->ExpectEOF());
  return r;
}

StatusOr<ResultSetKeyspaceResp> ParseResultSetKeyspace(FrameBodyDecoder* decoder) {
  ResultSetKeyspaceResp r;
  PX_ASSIGN_OR_RETURN(r.keyspace_name, decoder->ExtractString());
  PX_RETURN_IF_ERROR(decoder->ExpectEOF());
  return r;
}

StatusOr<ResultPreparedResp> ParseResultPrepared(FrameBodyDecoder* decoder) {
  ResultPreparedResp r;
  PX_ASSIGN_OR_RETURN(r.id, decoder->ExtractShortBytes());
  // Note that two metadata are sent back. The first communicates the col specs for the Prepared
  // statement, while the second communicates the metadata for future EXECUTE statements.
  PX_ASSIGN_OR_RETURN(r.metadata, decoder->ExtractResultMetadata(/* has_pk */ true));
  PX_ASSIGN_OR_RETURN(r.result_metadata, decoder->ExtractResultMetadata());
  PX_RETURN_IF_ERROR(decoder->ExpectEOF());
  return r;
}

StatusOr<ResultSchemaChangeResp> ParseResultSchemaChange(FrameBodyDecoder* decoder) {
  ResultSchemaChangeResp r;
  PX_ASSIGN_OR_RETURN(r.sc, decoder->ExtractSchemaChange());
  PX_RETURN_IF_ERROR(decoder->ExpectEOF());
  return r;
}

}  // namespace

StatusOr<ResultResp> ParseResultResp(Frame* frame) {
  ResultResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(int32_t kind_raw, decoder.ExtractInt());
  PX_ASSIGN_OR_RETURN(r.kind, EnumCast<ResultRespKind>(kind_raw));

  switch (r.kind) {
    case ResultRespKind::kVoid: {
      PX_ASSIGN_OR_RETURN(r.resp, ParseResultVoid(&decoder));
      break;
    }
    case ResultRespKind::kRows: {
      PX_ASSIGN_OR_RETURN(r.resp, ParseResultRows(&decoder));
      break;
    }
    case ResultRespKind::kSetKeyspace: {
      PX_ASSIGN_OR_RETURN(r.resp, ParseResultSetKeyspace(&decoder));
      break;
    }
    case ResultRespKind::kPrepared: {
      PX_ASSIGN_OR_RETURN(r.resp, ParseResultPrepared(&decoder));
      break;
    }
    case ResultRespKind::kSchemaChange: {
      PX_ASSIGN_OR_RETURN(r.resp, ParseResultSchemaChange(&decoder));
      break;
    }
    default:
      // EnumCast should ensure we never get here.
      LOG(DFATAL) << absl::Substitute("Unrecognized ResultRespKind $0", static_cast<int>(r.kind));
  }

  return r;
}

StatusOr<EventResp> ParseEventResp(Frame* frame) {
  EventResp r;
  FrameBodyDecoder decoder(*frame);
  PX_ASSIGN_OR_RETURN(r.event_type, decoder.ExtractString());

  if (r.event_type == "TOPOLOGY_CHANGE" || r.event_type == "STATUS_CHANGE") {
    PX_ASSIGN_OR_RETURN(r.change_type, decoder.ExtractString());
    PX_ASSIGN_OR_RETURN(r.addr, decoder.ExtractInet());
    PX_RETURN_IF_ERROR(decoder.ExpectEOF());
    return r;
  } else if (r.event_type == "SCHEMA_CHANGE") {
    PX_ASSIGN_OR_RETURN(r.sc, decoder.ExtractSchemaChange());
    PX_RETURN_IF_ERROR(decoder.ExpectEOF());
    return r;
  }

  return error::Internal("Unknown event_type $0", r.event_type);
}

}  // namespace cass
}  // namespace protocols
}  // namespace stirling
}  // namespace px
