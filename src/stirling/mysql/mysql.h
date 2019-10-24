#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"
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

inline MySQLEventType DecodeCommand(uint8_t command) {
  return static_cast<MySQLEventType>(command);
}

inline std::string CommandToString(MySQLEventType command) {
  return std::string(1, static_cast<char>(command));
}

// Response types
// https://dev.mysql.com/doc/internals/en/generic-response-packets.html
constexpr uint8_t kRespHeaderEOF = 0xfe;
constexpr uint8_t kRespHeaderErr = 0xff;
constexpr uint8_t kRespHeaderOK = 0x00;

// Column Types
// https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnType
constexpr uint8_t kColTypeTiny = 0x01;
constexpr uint8_t kColTypeShort = 0x02;
constexpr uint8_t kColTypeLong = 0x03;
constexpr uint8_t kColTypeFloat = 0x04;
constexpr uint8_t kColTypeDouble = 0x05;
constexpr uint8_t kColTypeTimeStamp = 0x07;
constexpr uint8_t kColTypeLongLong = 0x08;
constexpr uint8_t kColTypeDate = 0x0a;
constexpr uint8_t kColTypeDateTime = 0x0c;
constexpr uint8_t kColTypeNewDecimal = 0xf6;
constexpr uint8_t kColTypeBlob = 0xfc;
constexpr uint8_t kColTypeVarString = 0xfd;
constexpr uint8_t kColTypeString = 0xfe;

constexpr int kPacketHeaderLength = 4;

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
 * Raw MySQLPacket from MySQL Parser
 */
struct Packet {
  uint64_t timestamp_ns;
  std::chrono::time_point<std::chrono::steady_clock> creation_timestamp;

  uint8_t sequence_id;
  // TODO(oazizi): Convert to std::basic_string<uint8_t>.
  std::string msg;

  size_t ByteSize() const { return sizeof(Packet) + msg.size(); }
};

/**
 * Column definition is not parsed right now, but may be further parsed in the future.
 * https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnDefinition
 */
// TODO(chengruizhe): Parse Column definition packets.
struct ColDefinition {
  std::string msg;
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
 * https://dev.mysql.com/doc/internals/en/com-query-response.html#packet-Protocol::ColumnType
 */
enum class StmtExecuteParamType {
  kUnknown = 0,
  kString,
  kTiny,
  kShort,
  kLong,
  kLongLong,
  kFloat,
  kDouble,
  kNull,
  kDateTime,
};

/**
 * A parameter in StmtExecuteRequest.
 */
struct ParamPacket {
  StmtExecuteParamType type;
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
  std::vector<ParamPacket> params;
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
 * for the Stmtprepare event when a StmtExecute is received. cllient_deprecate_eof indicates
 * whether the ClientDeprecateEOF Flag is set.
 */
struct State {
  std::map<int, PreparedStatement> prepared_statements;
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

enum class MySQLRespStatus { kUnknown, kOK, kErr };

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
using Record = ReqRespPair<MySQLRequest, MySQLResponse>;

//-----------------------------------------------------------------------------
// Packet identification functions
//-----------------------------------------------------------------------------

/**
 * The following functions check whether a Packet is of a certain type, based on the prefixed
 * defined in mysql.h.
 */
bool IsEOFPacket(const Packet& packet);
bool IsErrPacket(const Packet& packet);
bool IsOKPacket(const Packet& packet);
bool IsLengthEncodedIntPacket(const Packet& packet);
bool IsColumnDefPacket(const Packet& packet);
bool IsResultsetRowPacket(const Packet& packet, bool client_deprecate_eof);
bool IsStmtPrepareOKPacket(const Packet& packet);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
