#pragma once

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * The MySQL parsing structure has 3 different levels of abstraction. From low to high level:
 * 1. MySQL Packet (Output of MySQL Parser). The content of it is not parsed.
 *    https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * 2. MySQL Message, a Request or Response, consisting of one or more MySQL Packets. It contains
 * parsed out fields based on the type of request/response.
 * 3. MySQL ReqRespEvent contains a request and response pair. It owns unique ptrs to
 * its request and response, and a MySQLEventType.
 *
 * A MySQL ReqRespEvent can be a standalone entry, or multiple can be combined to form one entry
 * in the table store. For events other than Stmt Prepare/Execute, an event contains all info in
 * this entry. A Stmt Prepare Event can be followed by multiple Stmt Execute Events, each generating
 * a new query with different params.
 */

//-----------------------------------------------------------------------------
// Packet Level Definitions
//-----------------------------------------------------------------------------

// Command Types
// https://dev.mysql.com/doc/internals/en/command-phase.html
constexpr uint8_t kComSleep = 0x00;
constexpr uint8_t kComQuit = 0x01;
constexpr uint8_t kComInitDB = 0x02;
constexpr uint8_t kComQuery = 0x03;
constexpr uint8_t kComFieldList = 0x04;
constexpr uint8_t kComCreateDB = 0x05;
constexpr uint8_t kComDropDB = 0x06;
constexpr uint8_t kComRefresh = 0x07;
constexpr uint8_t kComShutdown = 0x08;
constexpr uint8_t kComStatistics = 0x09;
constexpr uint8_t kComProcessInfo = 0x0a;
constexpr uint8_t kComConnect = 0x0b;
constexpr uint8_t kComProcessKill = 0x0c;
constexpr uint8_t kComDebug = 0x0d;
constexpr uint8_t kComPing = 0x0e;
constexpr uint8_t kComTime = 0x0f;
constexpr uint8_t kComDelayedInsert = 0x10;
constexpr uint8_t kComChangeUser = 0x11;
constexpr uint8_t kComBinlogDump = 0x12;
constexpr uint8_t kComTableDump = 0x13;
constexpr uint8_t kComConnectOut = 0x14;
constexpr uint8_t kComRegisterSlave = 0x15;
constexpr uint8_t kComStmtPrepare = 0x16;
constexpr uint8_t kComStmtExecute = 0x17;
constexpr uint8_t kComStmtSendLongData = 0x18;
constexpr uint8_t kComStmtClose = 0x19;
constexpr uint8_t kComStmtReset = 0x1a;
constexpr uint8_t kComSetOption = 0x1b;
constexpr uint8_t kComStmtFetch = 0x1c;
constexpr uint8_t kComDaemon = 0x1d;
constexpr uint8_t kComBinlogDumpGTID = 0x1e;
constexpr uint8_t kComResetConnection = 0x1f;

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

enum class MySQLEventType {
  kUnknown,
  kSleep,
  kQuit,
  kInitDB,
  kQuery,
  kFieldList,
  kCreateDB,
  kDropDB,
  kRefresh,
  kShutdown,
  kStatistics,
  kProcessInfo,
  kConnect,
  kProcessKill,
  kDebug,
  kPing,
  kTime,
  kDelayedInsert,
  kChangeUser,
  kBinlogDump,
  kTableDump,
  kConnectOut,
  kRegisterSlave,
  kStmtPrepare,
  kStmtExecute,
  kStmtSendLongData,
  kStmtClose,
  kStmtReset,
  kSetOption,
  kStmtFetch,
  kDaemon,
  kBinlogDumpGTID,
  kResetConnection,
};

inline MySQLEventType DecodeEventType(char command_byte) {
  switch (command_byte) {
    case kComSleep:
      return MySQLEventType::kSleep;
    case kComQuit:
      return MySQLEventType::kQuit;
    case kComInitDB:
      return MySQLEventType::kInitDB;
    case kComQuery:
      return MySQLEventType::kQuery;
    case kComFieldList:
      return MySQLEventType::kFieldList;
    case kComCreateDB:
      return MySQLEventType::kCreateDB;
    case kComDropDB:
      return MySQLEventType::kDropDB;
    case kComRefresh:
      return MySQLEventType::kRefresh;
    case kComShutdown:
      return MySQLEventType::kShutdown;
    case kComStatistics:
      return MySQLEventType::kStatistics;
    case kComProcessInfo:
      return MySQLEventType::kProcessInfo;
    case kComConnect:
      return MySQLEventType::kConnect;
    case kComProcessKill:
      return MySQLEventType::kProcessKill;
    case kComDebug:
      return MySQLEventType::kDebug;
    case kComPing:
      return MySQLEventType::kPing;
    case kComTime:
      return MySQLEventType::kTime;
    case kComDelayedInsert:
      return MySQLEventType::kDelayedInsert;
    case kComChangeUser:
      return MySQLEventType::kChangeUser;
    case kComBinlogDump:
      return MySQLEventType::kBinlogDump;
    case kComTableDump:
      return MySQLEventType::kTableDump;
    case kComConnectOut:
      return MySQLEventType::kConnectOut;
    case kComRegisterSlave:
      return MySQLEventType::kRegisterSlave;
    case kComStmtPrepare:
      return MySQLEventType::kStmtPrepare;
    case kComStmtExecute:
      return MySQLEventType::kStmtExecute;
    case kComStmtSendLongData:
      return MySQLEventType::kStmtSendLongData;
    case kComStmtClose:
      return MySQLEventType::kStmtClose;
    case kComStmtReset:
      return MySQLEventType::kStmtReset;
    case kComSetOption:
      return MySQLEventType::kSetOption;
    case kComStmtFetch:
      return MySQLEventType::kStmtFetch;
    case kComDaemon:
      return MySQLEventType::kDaemon;
    case kComBinlogDumpGTID:
      return MySQLEventType::kBinlogDumpGTID;
    case kComResetConnection:
      return MySQLEventType::kResetConnection;
    default:
      return MySQLEventType::kUnknown;
  }
}

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
  // TODO(oazizi): Convert to std::basic_string<uint8_t>.
  std::string msg;
  MySQLEventType type = MySQLEventType::kUnknown;

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

enum class RequestType { kUnknown = 0, kStringRequest, kStmtExecuteRequest, kStmtCloseRequest };

enum class ResponseType {
  kUnknown = 0,
  kStmtPrepareOKResponse,
  kResultset,
  kErrResponse,
  kOKResponse
};

/**
 * Response is a base for different kinds of MySQL Responses. They can consist of single
 * or multiple packets.
 */
class Response {
 public:
  virtual ~Response() = default;
  // TODO(chengruizhe): Remove default ctor when handle functions are implemented. Same below.
  Response() = default;
  ResponseType type() const { return type_; }

 protected:
  explicit Response(ResponseType type) : type_(type) {}

 private:
  ResponseType type_;
};

/**
 * https://dev.mysql.com/doc/internals/en/com-stmt-prepare-response.html
 */
class StmtPrepareOKResponse : public Response {
 public:
  StmtPrepareOKResponse() = default;
  StmtPrepareOKResponse(StmtPrepareRespHeader resp_header,
                        const std::vector<ColDefinition>& col_defs,
                        const std::vector<ColDefinition>& param_defs)
      : Response(ResponseType::kStmtPrepareOKResponse),
        resp_header_(resp_header),
        col_defs_(std::move(col_defs)),
        param_defs_(std::move(param_defs)) {}

  const StmtPrepareRespHeader& resp_header() const { return resp_header_; }
  const std::vector<ColDefinition>& col_defs() const { return col_defs_; }
  const std::vector<ColDefinition>& param_defs() const { return param_defs_; }

 private:
  StmtPrepareRespHeader resp_header_;
  std::vector<ColDefinition> col_defs_;
  std::vector<ColDefinition> param_defs_;
};

/**
 * A set of MySQL Query results. Contains a vector of resultset rows.
 * https://dev.mysql.com/doc/internals/en/binary-protocol-resultset.html
 */
// TODO(chengruizhe): Same as above. Differentiate binary Resultset and text Resultset.
class Resultset : public Response {
 public:
  Resultset() = default;
  // num_col could diverge from size of col_defs if packet is lost. Keeping it for detection
  // purposes.
  explicit Resultset(const int num_col, const std::vector<ColDefinition>& col_defs,
                     const std::vector<ResultsetRow>& results)
      : Response(ResponseType::kResultset),
        num_col_(num_col),
        col_defs_(col_defs),
        results_(results) {}

  int num_col() const { return num_col_; }
  const std::vector<ColDefinition>& col_defs() const { return col_defs_; }
  const std::vector<ResultsetRow>& results() const { return results_; }

 private:
  int num_col_;
  std::vector<ColDefinition> col_defs_;
  std::vector<ResultsetRow> results_;
};

/**
 *
 * https://dev.mysql.com/doc/internals/en/packet-ERR_Packet.html
 */
class ErrResponse : public Response {
 public:
  ErrResponse() = default;
  ErrResponse(int error_code, std::string_view msg)
      : Response(ResponseType::kErrResponse), error_code_(error_code), error_message_(msg) {}

  int error_code() const { return error_code_; }
  std::string_view error_message() const { return error_message_; }

 private:
  int error_code_;
  std::string error_message_;
};

/**
 * A generic OK Response. Returned when there is no result to be returned.
 * https://dev.mysql.com/doc/internals/en/packet-OK_Packet.html
 */
class OKResponse : public Response {
 public:
  OKResponse() : Response(ResponseType::kOKResponse) {}
};

/**
 * Request is a base for different kinds of MySQL requests. It has a single packet.
 */
class Request {
 public:
  virtual ~Request() = default;
  Request() = default;
  RequestType type() const { return type_; }

 protected:
  explicit Request(RequestType type) : type_(type) {}

 private:
  RequestType type_;
};

/**
 * StringRequest is a MySQL Request with a string as its message.
 * Most commonly StmtPrepare or Query requests, but also can be CreateDB.
 */
class StringRequest : public Request {
 public:
  StringRequest() = default;
  explicit StringRequest(std::string_view msg) : Request(RequestType::kStringRequest), msg_(msg) {}

  const std::string_view msg() const { return msg_; }

 private:
  std::string msg_;
};

class StmtExecuteRequest : public Request {
 public:
  StmtExecuteRequest() = default;
  explicit StmtExecuteRequest(int stmt_id, const std::vector<ParamPacket>& params)
      : Request(RequestType::kStmtExecuteRequest), stmt_id_(stmt_id), params_(std::move(params)) {}

  int stmt_id() const { return stmt_id_; }
  const std::vector<ParamPacket>& params() const { return params_; }

 private:
  int stmt_id_;
  std::vector<ParamPacket> params_;
};

class StmtCloseRequest : public Request {
 public:
  StmtCloseRequest() = default;
  explicit StmtCloseRequest(int stmt_id)
      : Request(RequestType::kStmtCloseRequest), stmt_id_(stmt_id) {}

  int stmt_id() const { return stmt_id_; }

 private:
  int stmt_id_;
};

//-----------------------------------------------------------------------------
// Event Level Structs
//-----------------------------------------------------------------------------

/**
 * ReqRespEvent holds a request and response pair, e.g. Stmt Prepare, Stmt Execute, Query
 */
class ReqRespEvent {
 public:
  ReqRespEvent() = default;
  explicit ReqRespEvent(MySQLEventType event_type, std::unique_ptr<Request> request,
                        std::unique_ptr<Response> response)
      : event_type_(event_type), request_(std::move(request)), response_(std::move(response)) {}

  MySQLEventType event_type() { return event_type_; }

  Request* request() const { return request_.get(); }

  Response* response() const { return response_.get(); }

 private:
  MySQLEventType event_type_;
  std::unique_ptr<Request> request_ = nullptr;
  std::unique_ptr<Response> response_ = nullptr;
};

//-----------------------------------------------------------------------------
// Table Store Entry Level Structs
//-----------------------------------------------------------------------------

/**
 *  MySQL Entry is emitted by Stitch functions, and will be appended to the table store.
 */
enum class MySQLEntryStatus { kUnknown, kOK, kErr };

struct Entry {
  std::string msg;
  MySQLEntryStatus status;
  uint64_t req_timestamp_ns;
};

enum class FlagStatus { kUnknown = 0, kSet, kNotSet };

/**
 * State stores a map of stmt_id to active StmtPrepare event. It's used to be looked up
 * for the Stmtprepare event when a StmtExecute is received. cllient_deprecate_eof indicates
 * whether the ClientDeprecateEOF Flag is set.
 */
struct State {
  std::map<int, mysql::ReqRespEvent> prepare_events;
  FlagStatus client_deprecate_eof;
};

/**
 * The following functions check whether a Packet is of a certain type, based on the prefixed
 * defined in mysql.h.
 */
bool IsEOFPacket(const Packet& packet);
bool IsErrPacket(const Packet& packet);
bool IsOKPacket(const Packet& packet);

/**
 * Checks whether an EOF packet is present and pops it off if it is.
 */
void ProcessEOFPacket(std::deque<Packet>* resp_packets);

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
