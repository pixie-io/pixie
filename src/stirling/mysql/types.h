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
#include "src/stirling/common/event_parser.h"  // For FrameBase
#include "src/stirling/common/utils.h"
#include "src/stirling/utils/req_resp_pair.h"

namespace pl {
namespace stirling {
namespace mysql {

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

struct Packet : public stirling::FrameBase {
  uint8_t sequence_id = 0;
  // TODO(oazizi): Convert to std::basic_string<uint8_t>.
  std::string msg;

  size_t ByteSize() const override { return sizeof(Packet) + msg.size(); }
};

//-----------------------------------------------------------------------------
// Packet Level Definitions
//-----------------------------------------------------------------------------

// Command Types
// https://dev.mysql.com/doc/internals/en/command-phase.html
enum class MySQLEventType : char {
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
  std::optional<MySQLEventType> command_type_option =
      magic_enum::enum_cast<MySQLEventType>(command_byte);
  if (!command_type_option.has_value()) {
    return false;
  }

  // The following are internal commands, and should not be sent on the connection.
  // In some sense, they are a valid part of the protocol, as the server will properly respond with
  // error. But for the sake of identifying mis-classified MySQL connections, it helps to call these
  // out as invalid commands.
  MySQLEventType command_type = command_type_option.value();
  if (command_type == MySQLEventType::kSleep || command_type == MySQLEventType::kTime ||
      command_type == MySQLEventType::kDelayedInsert ||
      command_type == MySQLEventType::kConnectOut || command_type == MySQLEventType::kDaemon) {
    return false;
  }

  return true;
}

inline MySQLEventType DecodeCommand(uint8_t command) {
  return static_cast<MySQLEventType>(command);
}

inline std::string CommandToString(MySQLEventType command) {
  return std::string(1, static_cast<char>(command));
}

constexpr int kMaxMySQLPacketLength = (1 << 24) - 1;

struct NumberRange {
  int min;
  int max;
};

inline std::vector<NumberRange> InitMySQLCommandLengths() {
  std::vector<NumberRange> cmd_length_ranges(magic_enum::enum_count<mysql::MySQLEventType>());

  cmd_length_ranges[static_cast<int>(MySQLEventType::kSleep)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kQuit)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kInitDB)] = {1, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kQuery)] = {1, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kFieldList)] = {2, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kCreateDB)] = {1, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kDropDB)] = {1, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kRefresh)] = {2, 2};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kShutdown)] = {1, 2};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStatistics)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kProcessInfo)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kConnect)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kProcessKill)] = {1, 5};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kDebug)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kPing)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kTime)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kDelayedInsert)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kChangeUser)] = {4, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kBinlogDump)] = {11, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kTableDump)] = {3, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kConnectOut)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kRegisterSlave)] = {18, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtPrepare)] = {1, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtExecute)] = {10, kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtSendLongData)] = {7,
                                                                            kMaxMySQLPacketLength};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtClose)] = {5, 5};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtReset)] = {5, 5};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kSetOption)] = {3, 3};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kStmtFetch)] = {9, 9};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kDaemon)] = {1, 1};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kBinlogDumpGTID)] = {19, 19};
  cmd_length_ranges[static_cast<int>(MySQLEventType::kResetConnection)] = {1, 1};

  return cmd_length_ranges;
}

inline std::vector<NumberRange> kMySQLCommandLengths = InitMySQLCommandLengths();

// Response types
// https://dev.mysql.com/doc/internals/en/generic-response-packets.html
constexpr uint8_t kRespHeaderEOF = 0xfe;
constexpr uint8_t kRespHeaderErr = 0xff;
constexpr uint8_t kRespHeaderOK = 0x00;

// Column Types
// https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnType
enum class MySQLColType : uint8_t {
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
  MySQLColType column_type;
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
  MySQLColType type;
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

enum class MySQLRespStatus { kUnknown, kNone, kOK, kErr };

struct MySQLRequest {
  // MySQL command. See MySQLEventType.
  MySQLEventType cmd;

  // The body of the request, if request has a single string parameter. Otherwise empty for now.
  std::string msg;

  // Timestamp of the request packet.
  uint64_t timestamp_ns;
};

struct MySQLResponse {
  // MySQL response status: OK, ERR or Unknown.
  MySQLRespStatus status;

  // Any relevant response message.
  std::string msg;

  // Timestamp of the last response packet.
  uint64_t timestamp_ns;
};

/**
 *  Record is the primary output of the mysql parser.
 */
struct Record {
  MySQLRequest req;
  MySQLResponse resp;

  // Debug information that we want to pass up this record.
  // Used to record info/warnings.
  // Only pushed to table store on debug builds.
  std::string px_info = "";
};

struct ProtocolTraits {
  using frame_type = Packet;
  using record_type = Record;
  using state_type = StateWrapper;
};

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
