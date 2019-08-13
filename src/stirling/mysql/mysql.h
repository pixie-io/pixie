#pragma once
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "src/common/base/base.h"

namespace pl {
namespace stirling {
namespace mysql {

/**
 * The MySQL parsing structure has 4 different levels of abstraction. From low to high level:
 * 1. Socket Data Event (Raw data from BPF).
 * 2. MySQL Packet (Output of MySQL Parser). The content of it is not parsed.
 *    https://dev.mysql.com/doc/internals/en/mysql-packet.html
 * 3. MySQL Message, a Request or Response, consisting of one or more MySQL Packets. It contains
 * parsed out fields based on the type of request/response.
 * 4. MySQL ReqRespEvent contains a request and response pair. It owns unique ptrs to
 * its request and response, and a MySQLEventType.
 *
 * A MySQL ReqRespEvent can be a standalone entry, or multiple can be combined to form one entry
 * in the table store. For events other than Stmt Prepare/Execute, an event contains all info in
 * this entry. A Stmt Prepare Event can be followed by multiple Stmt Execute Events, each generating
 * a new query with different params.
 */

inline constexpr char kEOFPrefix = '\xfe';
inline constexpr char kErrPrefix = '\xff';
inline constexpr char kOKPrefix = '\x00';

inline constexpr int kStmtIDStartOffset = 1;
inline constexpr int kStmtIDBytes = 4;
inline constexpr int kFlagsBytes = 1;
inline constexpr int kIterationCountBytes = 4;

inline constexpr char kNewDecimalPrefix = '\xf6';
inline constexpr char kBlobPrefix = '\xfc';
inline constexpr char kVarStringPrefix = '\xfd';
inline constexpr char kStringPrefix = '\xfe';

// TODO(chengruizhe): Switch prefix to char.
inline constexpr ConstStrView kStmtPreparePrefix = "\x16";
inline constexpr ConstStrView kStmtExecutePrefix = "\x17";
inline constexpr ConstStrView kQueryPrefix = "\x03";

inline constexpr char kLencIntPrefix2b = '\xfc';
inline constexpr char kLencIntPrefix3b = '\xfd';
inline constexpr char kLencIntPrefix8b = '\xfe';

enum class MySQLEventType { kUnknown, kComStmtPrepare, kComStmtExecute, kComQuery };

/**
 * Raw MySQLPacket from MySQL Parser
 */
struct Packet {
  uint64_t timestamp_ns;
  std::string msg;
  MySQLEventType type = MySQLEventType::kUnknown;
};

//-----------------------------------------------------------------------------
// Packet Level Structs
//-----------------------------------------------------------------------------

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
  kLong,
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

enum class RequestType { kUnknown = 0, kStringRequest, kStmtExecuteRequest };

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
