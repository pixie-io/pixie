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

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/event_parser.h"  // For FrameBase
#include "src/stirling/utils/utils.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mysql {

// This automatically provides an operator<< when ToString() is defined.
// Particularly useful for gtest.
using ::px::operator<<;

/**
 * The MySQL parsing structure has 3 different levels of abstraction. From low to high level:
 * 1. MySQL Packet (Output of MySQL Parser). The content of it is not parsed.
 *    https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * 2. MySQL Message, a Request or Response, consisting of one or more MySQL Packets. It contains
 * parsed out fields based on the type of request/response.
 * 3. MySQL Event, containing a request and response pair.
 */

// TODO(oazizi): Move relevant public types to mysql_parse.h and mysql_stitcher.h,
//               as appropriate.

//-----------------------------------------------------------------------------
// Raw MySQLPacket from MySQL Parser
//-----------------------------------------------------------------------------

struct Packet : public FrameBase {
  uint8_t sequence_id = 0;
  std::string msg;

  size_t ByteSize() const override { return sizeof(Packet) + msg.size(); }
};

//-----------------------------------------------------------------------------
// Packet Level Definitions
//-----------------------------------------------------------------------------

// Command Types
// https://dev.mysql.com/doc/internals/en/command-phase.html
enum class Command : char {
  kSleep = 0x00,
  kQuit = 0x01,
  kInitDB = 0x02,
  kQuery = 0x03,
  kFieldList = 0x04,
  kCreateDB = 0x05,
  kDropDB = 0x06,
  kRefresh = 0x07,
  kShutdown = 0x08,
  kStatistics = 0x09,
  kProcessInfo = 0x0a,
  kConnect = 0x0b,
  kProcessKill = 0x0c,
  kDebug = 0x0d,
  kPing = 0x0e,
  kTime = 0x0f,
  kDelayedInsert = 0x10,
  kChangeUser = 0x11,
  kBinlogDump = 0x12,
  kTableDump = 0x13,
  kConnectOut = 0x14,
  kRegisterSlave = 0x15,
  kStmtPrepare = 0x16,
  kStmtExecute = 0x17,
  kStmtSendLongData = 0x18,
  kStmtClose = 0x19,
  kStmtReset = 0x1a,
  kSetOption = 0x1b,
  kStmtFetch = 0x1c,
  kDaemon = 0x1d,
  kBinlogDumpGTID = 0x1e,
  kResetConnection = 0x1f,
};

constexpr uint8_t kMaxCommandValue = 0x1f;

inline bool IsValidCommand(uint8_t command_byte) {
  std::optional<Command> command_type_option = magic_enum::enum_cast<Command>(command_byte);
  if (!command_type_option.has_value()) {
    return false;
  }

  // The following are internal commands, and should not be sent on the connection.
  // In some sense, they are a valid part of the protocol, as the server will properly respond with
  // error. But for the sake of identifying mis-classified MySQL connections, it helps to call these
  // out as invalid commands.
  Command command_type = command_type_option.value();
  if (command_type == Command::kSleep || command_type == Command::kTime ||
      command_type == Command::kDelayedInsert || command_type == Command::kConnectOut ||
      command_type == Command::kDaemon) {
    return false;
  }

  return true;
}

inline Command DecodeCommand(uint8_t command) { return static_cast<Command>(command); }

inline std::string CommandToString(Command command) {
  return std::string(1, static_cast<char>(command));
}

constexpr int kMaxPacketLength = (1 << 24) - 1;

struct NumberRange {
  int min;
  int max;
};

inline std::vector<NumberRange> InitCommandLengths() {
  std::vector<NumberRange> cmd_length_ranges(magic_enum::enum_count<mysql::Command>());

  cmd_length_ranges[static_cast<int>(Command::kSleep)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kQuit)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kInitDB)] = {1, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kQuery)] = {1, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kFieldList)] = {2, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kCreateDB)] = {1, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kDropDB)] = {1, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kRefresh)] = {2, 2};
  cmd_length_ranges[static_cast<int>(Command::kShutdown)] = {1, 2};
  cmd_length_ranges[static_cast<int>(Command::kStatistics)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kProcessInfo)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kConnect)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kProcessKill)] = {1, 5};
  cmd_length_ranges[static_cast<int>(Command::kDebug)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kPing)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kTime)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kDelayedInsert)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kChangeUser)] = {4, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kBinlogDump)] = {11, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kTableDump)] = {3, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kConnectOut)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kRegisterSlave)] = {18, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kStmtPrepare)] = {1, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kStmtExecute)] = {10, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kStmtSendLongData)] = {7, kMaxPacketLength};
  cmd_length_ranges[static_cast<int>(Command::kStmtClose)] = {5, 5};
  cmd_length_ranges[static_cast<int>(Command::kStmtReset)] = {5, 5};
  cmd_length_ranges[static_cast<int>(Command::kSetOption)] = {3, 3};
  cmd_length_ranges[static_cast<int>(Command::kStmtFetch)] = {9, 9};
  cmd_length_ranges[static_cast<int>(Command::kDaemon)] = {1, 1};
  cmd_length_ranges[static_cast<int>(Command::kBinlogDumpGTID)] = {19, 19};
  cmd_length_ranges[static_cast<int>(Command::kResetConnection)] = {1, 1};

  return cmd_length_ranges;
}

inline std::vector<NumberRange> kMySQLCommandLengths = InitCommandLengths();

// Response types
// https://dev.mysql.com/doc/internals/en/generic-response-packets.html
constexpr uint8_t kRespHeaderEOF = 0xfe;
constexpr uint8_t kRespHeaderErr = 0xff;
constexpr uint8_t kRespHeaderOK = 0x00;

// Column Types
// https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnType
enum class ColType : uint8_t {
  kDecimal = 0x00,
  kTiny = 0x01,
  kShort = 0x02,
  kLong = 0x03,
  kFloat = 0x04,
  kDouble = 0x05,
  kNull = 0x06,
  kTimestamp = 0x07,
  kLongLong = 0x08,
  kInt24 = 0x09,
  kDate = 0x0a,
  kTime = 0x0b,
  kDateTime = 0x0c,
  kYear = 0x0d,
  kVarChar = 0x0f,
  kBit = 0x10,
  kNewDecimal = 0xf6,
  kEnum = 0xf7,
  kSet = 0xf8,
  kTinyBlob = 0xf9,
  kMediumBlob = 0xfa,
  kLongBlob = 0xfb,
  kBlob = 0xfc,
  kVarString = 0xfd,
  kString = 0xfe,
  kGeometry = 0xff,
};

// See https://dev.mysql.com/doc/internals/en/mysql-packet.html.
constexpr int kPacketHeaderLength = 4;

// Part of kPacketHeaderLength.
constexpr int kPayloadLengthLength = 3;

// Constants for StmtExecute packet, where the payload is as follows:
// bytes  description
//    1   [17] COM_STMT_EXECUTE
//    4   stmt-id
//    1   flags
//    4   iteration-count
constexpr int kStmtIDStartOffset = 1;
constexpr int kStmtIDBytes = 4;
constexpr int kFlagsBytes = 1;
constexpr int kIterationCountBytes = 4;

//-----------------------------------------------------------------------------
// Packet Level Structs
//-----------------------------------------------------------------------------

/**
 * Column definition is not parsed right now, but may be further parsed in the future.
 * https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnDefinition
 */
struct ColDefinition {
  std::string catalog;  // Always "def"
  std::string schema;
  std::string table;
  std::string org_table;
  std::string name;
  std::string org_name;
  int8_t next_length;  // Always 0x0c
  int16_t character_set;
  int32_t column_length;
  ColType column_type;
  int16_t flags;
  int8_t decimals;
};

/**
 * The first packet in a StmtPrepareOkResponse.
 */
struct StmtPrepareRespHeader {
  int stmt_id;
  size_t num_columns;
  size_t num_params;
  size_t warning_count;
};

/**
 * One row of data for each column in Resultset. Could be further parsed for results.
 * https://dev.mysql.com/doc/internals/en/com-query-response.html#text-resultset-row
 */
// TODO(chengruizhe): Differentiate binary Resultset Row and text Resultset Row.
struct ResultsetRow {
  std::string msg;
};

/**
 * A parameter in StmtExecuteRequest.
 */
struct StmtExecuteParam {
  ColType type;
  std::string value;
};

//-----------------------------------------------------------------------------
// Message Level Structs
//-----------------------------------------------------------------------------

/**
 * https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html
 */
struct StmtPrepareOKResponse {
  StmtPrepareRespHeader header;
  std::vector<ColDefinition> col_defs;
  std::vector<ColDefinition> param_defs;
};

/**
 * A set of MySQL Query results. Contains a vector of resultset rows.
 * https://dev.mysql.com/doc/internals/en/binary-protocol-resultset.html
 */
struct Resultset {
  // Keep num_col explicitly, since it could diverge from col_defs.size() if packet is lost.
  int num_col = 0;
  std::vector<ColDefinition> col_defs;
  std::vector<ResultsetRow> results;
};

/**
 * https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
 */
struct ErrResponse {
  int error_code = 0;
  std::string error_message;
};

/**
 * StringRequest is a MySQL Request with a string as its message.
 * Most commonly StmtPrepare or Query requests, but also can be CreateDB.
 */
struct StringRequest {
  std::string msg;
};

struct StmtExecuteRequest {
  int stmt_id;
  std::vector<StmtExecuteParam> params;
};

struct StmtCloseRequest {
  int stmt_id;
};

//-----------------------------------------------------------------------------
// State Structs
//-----------------------------------------------------------------------------

/**
 * PreparedStatement holds a prepared statement string, and a parsed response,
 * which contains the placeholder column definitions.
 */
struct PreparedStatement {
  std::string request;
  StmtPrepareOKResponse response;
};

/**
 * State stores a map of stmt_id to active StmtPrepare event. It's used to be looked up
 * for the StmtPrepare event when a StmtExecute is received.
 */
struct State {
  std::map<int, PreparedStatement> prepared_statements;
  // To prevent pushing data on mis-classified connections,
  // we start off in inactive state, which means no data will be pushed out.
  // Only on certain conditions, which increase our confidence that the data is indeed MySQL,
  // do we flip this switch, and start pushing data.
  bool active = false;
};

struct StateWrapper {
  State global;
  std::monostate send;
  std::monostate recv;
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

enum class RespStatus { kUnknown, kNone, kOK, kErr };

struct Request {
  // MySQL command. See Command.
  Command cmd;

  // The body of the request, if request has a single string parameter. Otherwise empty for now.
  std::string msg;

  // Timestamp of the request packet.
  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("timestamp=$0 cmd=$1 msg=$2", timestamp_ns, magic_enum::enum_name(cmd),
                            msg);
  }
};

struct Response {
  // MySQL response status: OK, ERR or Unknown.
  RespStatus status;

  // Any relevant response message.
  std::string msg;

  // Timestamp of the last response packet.
  uint64_t timestamp_ns;

  std::string ToString() const {
    return absl::Substitute("timestamp=$0 status=$1 msg=$2", timestamp_ns,
                            magic_enum::enum_name(status), msg);
  }
};

/**
 *  Record is the primary output of the mysql parser.
 */
struct Record {
  Request req;
  Response resp;

  // Debug information that we want to pass up this record.
  // Used to record info/warnings.
  // Only pushed to table store on debug builds.
  std::string px_info = "";

  std::string ToString() const {
    return absl::Substitute("req=[$0] resp=[$1]", req.ToString(), resp.ToString());
  }
};

using connection_id_t = uint16_t;
struct ProtocolTraits : public BaseProtocolTraits<Record> {
  using frame_type = Packet;
  using record_type = Record;
  using state_type = StateWrapper;
  using key_type = connection_id_t;
};

}  // namespace mysql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
