#include "src/stirling/pgsql/stitcher.h"

#include <string>
#include <variant>

#include "src/stirling/common/binary_decoder.h"
#include "src/stirling/pgsql/parse.h"

namespace pl {
namespace stirling {

namespace pgsql {

namespace {

void AdvanceIterBeyondTimestamp(MsgDeqIter* start, const MsgDeqIter& end, uint64_t ts) {
  while (*start != end && (*start)->timestamp_ns < ts) {
    ++(*start);
  }
}

template <typename TElemType>
class DequeView {
 public:
  using Iteraotr = typename std::deque<TElemType>::iterator;

  DequeView(Iteraotr begin, Iteraotr end)
      : size_(std::distance(begin, end)), begin_(begin), end_(end) {}

  const TElemType& operator[](size_t i) const { return *(begin_ + i); }
  const TElemType& Back() const { return *(end_ - 1); }
  const TElemType& Front() const { return *begin_; }
  size_t Size() const { return size_; }
  bool Empty() const { return size_ == 0; }

  const Iteraotr& Begin() const { return begin_; }
  const Iteraotr& End() const { return end_; }

 private:
  size_t size_ = 0;
  Iteraotr begin_;
  Iteraotr end_;
};

// Returns a list of messages that before ends with a kCmdComplete or kErrResp message.
DequeView<RegularMessage> GetCmdRespMsgs(MsgDeqIter* begin, MsgDeqIter end) {
  auto resp_iter = std::find_if(*begin, end, TagMatcher({Tag::kCmdComplete, Tag::kErrResp}));
  if (resp_iter == end) {
    return {end, end};
  }
  ++resp_iter;
  DequeView<RegularMessage> res(*begin, resp_iter);
  *begin = resp_iter;
  return res;
}

#define PL_ASSIGN_OR_RETURN_RES(expr, val_or, res) PL_ASSIGN_OR(expr, val_or, return res)

// Error message's payload has multiple following fields:
// | byte type | string value |
//
// TODO(yzhao): Do not call parsing functions. Check out frame_body_decoder.cc
// StatusOr<QueryReq> ParseQueryReq(Frame* frame) for parsing;
// and cql_stitcher.cc Status ProcessQueryReq(Frame* req_frame, Request* req) for stitching and
// formatting.
std::string_view FmtErrorResp(const RegularMessage& msg) {
  BinaryDecoder decoder(msg.payload);
  // Each field has at least 2 bytes, one for byte, another for string, which can be empty, but
  // always ends with '\0'.
  while (decoder.BufSize() >= 2) {
    PL_ASSIGN_OR_RETURN_RES(const char type, decoder.ExtractChar(), {});
    // See https://www.postgresql.org/docs/9.3/protocol-error-fields.html for the complete list of
    // error code.
    constexpr char kHumanReadableMessage = 'M';
    if (type == kHumanReadableMessage) {
      PL_ASSIGN_OR_RETURN_RES(std::string_view str_view, decoder.ExtractStringUntil('\0'), {});
      return str_view;
    }
  }
  return msg.payload;
}

// TODO(yzhao): Do not call parsing functions.
std::string FmtSelectResp(const DequeView<RegularMessage>& msgs) {
  auto row_desc_iter = std::find_if(msgs.Begin(), msgs.End(), TagMatcher({Tag::kRowDesc}));
  ECHECK(row_desc_iter != msgs.End()) << "Failed to find kRowDesc message in SELECT response.";

  std::string res;
  if (row_desc_iter != msgs.End()) {
    std::vector<std::string_view> row_descs = ParseRowDesc(row_desc_iter->payload);
    absl::StrAppend(&res, absl::StrJoin(row_descs, ","));
    absl::StrAppend(&res, "\n");
  }
  for (size_t i = 1; i < msgs.Size() - 1; ++i) {
    // kDataRow messages can mixed with other responses in an extended query, in which protocol
    // parsing, parameter binding, execution are separated into multiple request & response
    // exchanges.
    if (msgs[i].tag != Tag::kDataRow) {
      continue;
    }
    std::vector<std::optional<std::string_view>> data_row = ParseDataRow(msgs[i].payload);
    absl::StrAppend(&res,
                    absl::StrJoin(data_row, ",",
                                  [](std::string* out, const std::optional<std::string_view>& d) {
                                    out->append(d.has_value() ? d.value() : "[NULL]");
                                  }));
    absl::StrAppend(&res, "\n");
  }
  return res;
}

namespace cmd {

constexpr std::string_view kSelect = "SELECT";

}  // namespace cmd

// Query response messages end with a kCmdComplete message. Its payload determines the content prior
// to that message.
//
// See CommandComplete section in https://www.postgresql.org/docs/9.3/protocol-message-formats.html.
//
// TODO(yzhao): Format in JSON.
// TODO(yzhao): Do not call parsing code inside.
std::string FmtCmdResp(const DequeView<RegularMessage>& msgs) {
  const RegularMessage& cmd_complete_msg = msgs.Back();
  if (cmd_complete_msg.tag == Tag::kErrResp) {
    return std::string(FmtErrorResp(cmd_complete_msg));
  }
  // Non-SELECT response only has one kCmdComplete message. Output the payload directly.
  if (msgs.Size() == 1) {
    return msgs.Begin()->payload;
  }
  std::string res;
  if (absl::StartsWith(cmd_complete_msg.payload, cmd::kSelect)) {
    res = FmtSelectResp(msgs);
  }
  absl::StrAppend(&res, msgs.Back().payload);
  // TODO(yzhao): Need to test and handle other cases, if any.
  return res;
}

std::string_view StripZeroChar(std::string_view str) {
  while (!str.empty() && str.back() == '\0') {
    str.remove_suffix(1);
  }
  while (!str.empty() && str.front() == '\0') {
    str.remove_prefix(1);
  }
  return str;
}

void StripZeroChar(Record* record) {
  record->req.payload = StripZeroChar(record->req.payload);
  record->resp.payload = StripZeroChar(record->resp.payload);
}

}  // namespace

// Find the messages of query response. The result argument begin is advanced past the right most
// message being consumed.
//
// TODO(yzhao): Change this to use ContainerView<> as input.
StatusOr<RegularMessage> AssembleQueryResp(MsgDeqIter* begin, const MsgDeqIter& end) {
  DequeView<RegularMessage> msgs = GetCmdRespMsgs(begin, end);
  if (msgs.Empty()) {
    return error::InvalidArgument("Did not find kCmdComplete or kErrResp message");
  }
  std::string text = FmtCmdResp(msgs);
  RegularMessage msg = {};
  msg.timestamp_ns = msgs.Front().timestamp_ns;
  msg.payload = std::move(text);
  return msg;
}

// Returns all messages until the first kExecute message. Some of the returned messages are used to
// format a request message. As of 2020-04, only the first kParse message's payload is included in
// the request message, all others are discarded.
StatusOr<std::vector<RegularMessage>> GetParseReqMsgs(MsgDeqIter* begin, const MsgDeqIter& end) {
  auto kExec_iter = std::find_if(*begin, end, TagMatcher({Tag::kExecute}));
  if (kExec_iter == end) {
    return error::InvalidArgument("Could not find kExec message");
  }
  std::vector<RegularMessage> msgs;
  for (auto iter = *begin; iter != kExec_iter; ++iter) {
    msgs.push_back(std::move(*iter));
  }
  ++kExec_iter;
  *begin = kExec_iter;
  if (msgs.empty()) {
    return error::InvalidArgument("Did not find any messages");
  }
  return msgs;
}

Status HandleParse(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                   ParseReqResp* req_resp, State* state) {
  DCHECK_EQ(msg.tag, Tag::kParse);
  Parse parse;
  PL_RETURN_IF_ERROR(ParseParse(msg, &parse));

  auto iter = std::find_if(*resp_iter, end, TagMatcher({Tag::kParseComplete, Tag::kErrResp}));
  if (iter == end) {
    return error::NotFound("Did not find CmdComplete or ErrorResponse message");
  }

  *resp_iter = iter + 1;
  req_resp->req = parse;

  if (iter->tag == Tag::kParseComplete) {
    if (parse.stmt_name.empty()) {
      state->unnamed_statement = parse.query;
    } else {
      state->prepared_statements[parse.stmt_name] = parse.query;
    }
  }

  if (iter->tag == Tag::kErrResp) {
    ErrResp err_resp;
    PL_RETURN_IF_ERROR(ParseErrResp(iter->payload, &err_resp));
    req_resp->resp = err_resp;
  }

  return Status::OK();
}

namespace {

struct ErrFieldFormatter {
  void operator()(std::string* out, const ErrResp::Field& err_field) const {
    std::string_view field_name = magic_enum::enum_name(err_field.code);
    if (field_name.empty()) {
      std::string unknown_field_name = "Field:";
      unknown_field_name.append(1, static_cast<char>(err_field.code));
      out->append(absl::StrCat(unknown_field_name, "=", err_field.value));
    } else {
      out->append(absl::StrCat(field_name, "=", err_field.value));
    }
  }
};

}  // namespace

std::string FmtErrResp(const ErrResp& err_resp) {
  return absl::StrJoin(err_resp.fields, "\n", ErrFieldFormatter());
}

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps,
                                                   State* state) {
  std::vector<pgsql::Record> records;
  int error_count = 0;
  auto req_iter = reqs->begin();
  auto resp_iter = resps->begin();
  // PostgreSQL query mode:
  //   In-order mode: where one query (one regular message) is followed one response (possibly with
  //   multiple regular messages).
  //   The code now can handle this mode.
  //
  //   Batch mode: Multiple queries are batched into a list, and sent to server; the responses are
  //   sent back in the same order as their corresponding queries.
  //   The code now can handle this mode.
  //
  //   Pipeline mode: Seem supported in PostgreSQL.
  //   https://2ndquadrant.github.io/postgres/libpq-batch-mode.html
  //   mentions pipelining. But the details are not clear yet.
  //
  // TODO(yzhao): Research batch and pipeline mode and confirm their behaviors.
  while (req_iter != reqs->end() && resp_iter != resps->end()) {
    // First advance response iterator to be at or later than the request's time stamp.
    // This can:
    // * Skip kReadyForQuery message, which appears before any of request messages.
    AdvanceIterBeyondTimestamp(&resp_iter, resps->end(), req_iter->timestamp_ns);

    // TODO(yzhao): Use a map to encode request type and the actions to find the response.
    // So we can get rid of the switch statement. That also include AdvanceIterBeyondTimestamp()
    // into the handler functions, such that the logic is more grouped.
    switch (req_iter->tag) {
      case Tag::kReadyForQuery:
        VLOG(1) << "Skip Tag::kReadyForQuery.";
        break;
      // NOTE: kPasswd message is a response by client to the authenticate request.
      // But it was still classified as request to server from client, according to our model.
      // And we just ignore such message, and kAuth message as well.
      case Tag::kPasswd:
        // Ignore auth response.
        ++req_iter;
        break;
      case Tag::kQuery: {
        auto query_resp_or = AssembleQueryResp(&resp_iter, resps->end());
        if (query_resp_or.ok()) {
          Record record{std::move(*req_iter), query_resp_or.ConsumeValueOrDie()};
          StripZeroChar(&record);
          records.push_back(std::move(record));
        } else {
          ++error_count;
          VLOG(1) << "Failed to assemble query response message, status: "
                  << query_resp_or.ToString();
        }
        ++req_iter;
        break;
      }
      case Tag::kParse: {
        ParseReqResp req_resp;
        if (HandleParse(*req_iter, &resp_iter, resps->end(), &req_resp, state).ok()) {
          RegularMessage req;
          req.timestamp_ns = req_resp.req.timestamp_ns;
          req.payload = req_resp.req.query;

          RegularMessage resp;
          resp.payload =
              req_resp.resp.has_value() ? FmtErrResp(req_resp.resp.value()) : "PARSE COMPLETE";
          records.push_back({std::move(req), std::move(resp)});
        }

        auto req_msgs_or = GetParseReqMsgs(&req_iter, reqs->end());
        if (!req_msgs_or.ok()) {
          ++error_count;
          VLOG(1) << "Failed to assemble parse request messages, status: "
                  << req_msgs_or.ToString();
          break;
        }
        auto query_resp_or = AssembleQueryResp(&resp_iter, resps->end());
        if (!query_resp_or.ok()) {
          ++error_count;
          VLOG(1) << "Failed to assemble query response message, status: "
                  << query_resp_or.ToString();
          break;
        }
        Record record{std::move(req_msgs_or.ConsumeValueOrDie().front()),
                      query_resp_or.ConsumeValueOrDie()};
        StripZeroChar(&record);
        records.push_back(std::move(record));
        break;
      }
      default:
        LOG_FIRST_N(WARNING, 10) << "Unhandled or invalid tag: "
                                 << static_cast<char>(req_iter->tag);
        // By default for any message with unhandled or invalid tag, ignore and continue.
        // TODO(yzhao): Revise based on feedbacks.
        ++req_iter;
        break;
    }
  }
  reqs->erase(reqs->begin(), req_iter);
  resps->erase(resps->begin(), resp_iter);
  return {records, error_count};
}

}  // namespace pgsql

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps,
                                                   pgsql::StateWrapper* state) {
  return pgsql::ProcessFrames(reqs, resps, &state->global);
}

}  // namespace stirling
}  // namespace pl
