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

#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/stitcher.h"

#include <string>
#include <utility>
#include <variant>

#include <absl/strings/str_replace.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {

namespace {

template <typename TElemType>
class DequeView {
 public:
  using Iterator = typename std::deque<TElemType>::iterator;

  DequeView(Iterator begin, Iterator end)
      : size_(std::distance(begin, end)), begin_(begin), end_(end) {}

  const TElemType& operator[](size_t i) const { return *(begin_ + i); }

  const TElemType& Back() const { return *(end_ - 1); }

  const TElemType& Front() const { return *begin_; }

  size_t Size() const { return size_; }

  bool Empty() const { return size_ == 0; }

  const Iterator& begin() const { return begin_; }

  const Iterator& end() const { return end_; }

 private:
  size_t size_ = 0;
  Iterator begin_;
  Iterator end_;
};

// Query response messages end with a kCmdComplete message. Its payload determines the content prior
// to that message.
//
// See CommandComplete section in https://www.postgresql.org/docs/9.3/protocol-message-formats.html.
//
// TODO(yzhao): Format in JSON.
// TODO(yzhao): Do not call parsing code inside.

}  // namespace

// Find the messages of query response. The result argument begin is advanced past the right most
// message being consumed.
//
// TODO(yzhao): Change this to use ContainerView<> as input.
Status FillQueryResp(MsgDeqIter* resp_iter, const MsgDeqIter& end, QueryReqResp::QueryResp* resp) {
  bool found_row_desc = false;
  bool found_cmd_cmpl = false;
  bool found_err_resp = false;

  for (auto& iter = *resp_iter; iter != end; ++iter) {
    if (iter->tag == Tag::kCmdComplete) {
      found_cmd_cmpl = true;

      if (resp->timestamp_ns == 0) {
        resp->timestamp_ns = iter->timestamp_ns;
      }

      PX_RETURN_IF_ERROR(ParseCmdCmpl(*iter, &resp->cmd_cmpl));
      iter->consumed = true;
      break;
    }

    if (iter->tag == Tag::kErrResp) {
      found_err_resp = true;

      if (resp->timestamp_ns == 0) {
        resp->timestamp_ns = iter->timestamp_ns;
      }

      PX_RETURN_IF_ERROR(ParseErrResp(*iter, &resp->err_resp));
      iter->consumed = true;
      break;
    }

    if (iter->tag == Tag::kRowDesc) {
      found_row_desc = true;

      if (resp->timestamp_ns == 0) {
        resp->timestamp_ns = iter->timestamp_ns;
      }

      PX_RETURN_IF_ERROR(ParseRowDesc(*iter, &resp->row_desc));
      iter->consumed = true;
    }

    // The response to a SELECT query (or other queries that return row sets, such as EXPLAIN or
    // SHOW) normally consists of RowDescription, zero or more DataRow messages, and then
    // CommandComplete. Therefore we should not break out of the for loop once the first DataRow
    // message is parsed.
    if (iter->tag == Tag::kDataRow) {
      DataRow data_row;
      PX_RETURN_IF_ERROR(ParseDataRow(*iter, &data_row));
      resp->data_rows.push_back(std::move(data_row));
      iter->consumed = true;
    }
  }

  // TODO(ddelnano): This iterator incrementing should likely happen
  // after the InvalidArgument error is returned below. This should only
  // impact the case when there is an error, but we are leaving as is
  // avoid a larger change without further verification.
  if (*resp_iter != end) {
    ++(*resp_iter);
  }

  if (!found_row_desc && !(found_cmd_cmpl || found_err_resp)) {
    return error::InvalidArgument("Did not find kRowDesc and one of {kCmdComplete, kErrResp}");
  }

  return Status::OK();
}

Status HandleQuery(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                   QueryReqResp* req_resp) {
  CTX_DCHECK_EQ(msg.tag, Tag::kQuery);

  req_resp->req.timestamp_ns = msg.timestamp_ns;
  req_resp->req.query = msg.payload;

  RemoveRepeatingSuffix(&req_resp->req.query, '\0');

  // TODO(yzhao): Turn the rest code of this function to another function.

  bool found_resp = false;

  for (auto& iter = *resp_iter; iter != end; ++iter) {
    if (iter->tag == Tag::kEmptyQueryResponse) {
      found_resp = true;
      req_resp->resp.timestamp_ns = iter->timestamp_ns;
      iter->consumed = true;
      break;
    }

    if (iter->tag == Tag::kCmdComplete) {
      found_resp = true;
      PX_RETURN_IF_ERROR(ParseCmdCmpl(*iter, &req_resp->resp.cmd_cmpl));

      iter->consumed = true;
      req_resp->resp.timestamp_ns = req_resp->resp.cmd_cmpl.timestamp_ns;
      break;
    }

    if (iter->tag == Tag::kErrResp) {
      found_resp = true;
      PX_RETURN_IF_ERROR(ParseErrResp(*iter, &req_resp->resp.err_resp));

      iter->consumed = true;
      req_resp->resp.timestamp_ns = req_resp->resp.err_resp.timestamp_ns;
      break;
    }

    if (iter->tag == Tag::kRowDesc) {
      found_resp = true;
      req_resp->resp.cmd_cmpl.timestamp_ns = iter->timestamp_ns;
      PX_RETURN_IF_ERROR(ParseRowDesc(*iter, &req_resp->resp.row_desc));

      iter->consumed = true;
      req_resp->resp.timestamp_ns = req_resp->resp.row_desc.timestamp_ns;
    }

    // The response to a SELECT query (or other queries that return row sets, such as EXPLAIN or
    // SHOW) normally consists of RowDescription, zero or more DataRow messages, and then
    // CommandComplete. Therefore we should not break out of the for loop once the first DataRow
    // message is parsed.
    if (iter->tag == Tag::kDataRow) {
      DataRow data_row;
      PX_RETURN_IF_ERROR(ParseDataRow(*iter, &data_row));

      iter->consumed = true;
      req_resp->resp.timestamp_ns = data_row.timestamp_ns;
      req_resp->resp.data_rows.push_back(std::move(data_row));
    }
  }

  // TODO(ddelnano): This iterator incrementing should likely happen
  // after the InvalidArgument error is returned below. This should only
  // impact the case when there is an error, but we are leaving as is
  // avoid a larger change without further verification.
  if (*resp_iter != end) {
    ++(*resp_iter);
  }

  if (!found_resp) {
    return error::InvalidArgument("Did not find a valid query response.");
  }

  return Status::OK();
}

constexpr char kParseCmplText[] = "PARSE COMPLETE";

Status HandleParse(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                   ParseReqResp* req_resp, State* state) {
  CTX_DCHECK_EQ(msg.tag, Tag::kParse);
  Parse parse;
  PX_RETURN_IF_ERROR(ParseParse(msg, &parse));

  auto iter = std::find_if(*resp_iter, end, TagMatcher({Tag::kParseComplete, Tag::kErrResp}));
  if (iter == end) {
    return error::NotFound("Did not find CmdComplete or ErrorResponse message");
  }

  // Update the iterator for response messages.
  *resp_iter = iter + 1;
  req_resp->req = parse;
  req_resp->resp.timestamp_ns = iter->timestamp_ns;

  if (iter->tag == Tag::kParseComplete) {
    if (parse.stmt_name.empty()) {
      state->unnamed_statement = parse.query;
    } else {
      state->prepared_statements[parse.stmt_name] = parse.query;
    }
    req_resp->resp.msg = CmdCmpl{.timestamp_ns = iter->timestamp_ns, .cmd_tag = kParseCmplText};
  }

  if (iter->tag == Tag::kErrResp) {
    ErrResp err_resp;
    PX_RETURN_IF_ERROR(ParseErrResp(*iter, &err_resp));
    req_resp->resp.msg = err_resp;
  }
  iter->consumed = true;

  return Status::OK();
}

Status HandleBind(const RegularMessage& bind_msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                  BindReqResp* req_resp, State* state) {
  CTX_DCHECK_EQ(bind_msg.tag, Tag::kBind);

  BindRequest bind_req;
  PX_RETURN_IF_ERROR(ParseBindRequest(bind_msg, &bind_req));

  auto iter = std::find_if(*resp_iter, end, TagMatcher({Tag::kBindComplete, Tag::kErrResp}));
  if (iter == end) {
    return error::InvalidArgument("Could not find bind complete or error response message");
  }

  *resp_iter = iter + 1;

  req_resp->resp.timestamp_ns = iter->timestamp_ns;

  if (iter->tag == Tag::kBindComplete) {
    // Unnamed statement.
    if (bind_req.src_prepared_stat_name.empty()) {
      state->bound_statement = state->unnamed_statement;
    } else {
      if (!state->prepared_statements.contains(bind_req.src_prepared_stat_name)) {
        // TODO(yzhao): The code should handle the case where the previous Parse message was not
        // seen, i.e., state->prepared_statements does not contain the requested statement name.
        return error::InvalidArgument("Statement [name=$0] is not recorded",
                                      bind_req.src_prepared_stat_name);
      }
      state->bound_statement = state->prepared_statements[bind_req.src_prepared_stat_name];
    }
    state->bound_params = bind_req.params;
    req_resp->resp.msg = CmdCmpl{.timestamp_ns = iter->timestamp_ns, .cmd_tag = "BIND COMPLETE"};
  }

  req_resp->req = std::move(bind_req);

  if (iter->tag == Tag::kErrResp) {
    ErrResp err_resp;
    PX_RETURN_IF_ERROR(ParseErrResp(*iter, &err_resp));
    req_resp->resp.msg = std::move(err_resp);
  }

  iter->consumed = true;

  return Status::OK();
}

Status FillStmtDescResp(MsgDeqIter* resp_iter, const MsgDeqIter& end, DescReqResp::Resp* resp) {
  auto iter = std::find_if(*resp_iter, end, TagMatcher({Tag::kParamDesc, Tag::kErrResp}));

  if (iter == end) {
    return error::InvalidArgument("Could not find kParamDesc or kErrResp message");
  }

  resp->timestamp_ns = iter->timestamp_ns;

  *resp_iter = iter + 1;

  if (iter->tag == Tag::kErrResp) {
    resp->is_err_resp = true;
    PX_RETURN_IF_ERROR(ParseErrResp(*iter, &resp->err_resp));
    iter->consumed = true;
    return Status::OK();
  }

  PX_RETURN_IF_ERROR(ParseParamDesc(*iter, &resp->param_desc));
  if (++iter == end) {
    return error::InvalidArgument("Should have another message following kParamDesc");
  }

  *resp_iter = iter + 1;

  if (iter->tag == Tag::kRowDesc) {
    auto s = ParseRowDesc(*iter, &resp->row_desc);
    if (s.ok()) {
      iter->consumed = true;
    }
    return s;
  }

  if (iter->tag == Tag::kNoData) {
    resp->is_no_data = true;
    iter->consumed = true;
    return Status::OK();
  }

  return error::InvalidArgument("Expect kRowDesc or kNoData after kParamDesc, got: $0",
                                iter->ToString());
}

Status FillPortalDescResp(MsgDeqIter* resp_iter, const MsgDeqIter& end, DescReqResp::Resp* resp) {
  auto iter = std::find_if(*resp_iter, end, TagMatcher({Tag::kParamDesc, Tag::kErrResp}));

  if (iter == end) {
    return error::InvalidArgument("Could not find kParamDesc or kErrResp message");
  }

  *resp_iter = iter + 1;

  if (iter->tag == Tag::kErrResp) {
    resp->is_err_resp = true;
    PX_RETURN_IF_ERROR(ParseErrResp(*iter, &resp->err_resp));
    return Status::OK();
  }

  PX_RETURN_IF_ERROR(ParseRowDesc(*iter, &resp->row_desc));

  return Status::OK();
}

Status HandleDesc(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                  DescReqResp* req_resp) {
  CTX_DCHECK_EQ(msg.tag, Tag::kDesc);

  PX_RETURN_IF_ERROR(ParseDesc(msg, &req_resp->req));

  if (req_resp->req.type == Desc::Type::kStatement) {
    return FillStmtDescResp(resp_iter, end, &req_resp->resp);
  }

  if (req_resp->req.type == Desc::Type::kPortal) {
    return FillPortalDescResp(resp_iter, end, &req_resp->resp);
  }

  return error::InvalidArgument("Invalid describe target type, message: $0", msg.ToString());
}

Status HandleExecute(const RegularMessage& msg, MsgDeqIter* resps_begin,
                     const MsgDeqIter& resps_end, ExecReqResp* req_resp, State* state) {
  CTX_DCHECK_EQ(msg.tag, Tag::kExecute);

  req_resp->req.timestamp_ns = msg.timestamp_ns;
  req_resp->req.query = state->bound_statement;
  req_resp->req.params = state->bound_params;

  PX_RETURN_IF_ERROR(FillQueryResp(resps_begin, resps_end, &req_resp->resp));

  return Status::OK();
}

#define CALL_HANDLER(TReqRespType, expr)                  \
  TReqRespType req_resp;                                  \
  auto status = expr;                                     \
  if (status.ok()) {                                      \
    req.consumed = true;                                  \
    CTX_DCHECK_NE(req.timestamp_ns, 0U);                  \
    req.payload = req_resp.req.ToString();                \
    RegularMessage resp;                                  \
    resp.timestamp_ns = req_resp.resp.timestamp_ns;       \
    CTX_DCHECK_NE(resp.timestamp_ns, 0U);                 \
    resp.payload = req_resp.resp.ToString();              \
    records.push_back({std::move(req), std::move(resp)}); \
  } else {                                                \
    VLOG(2) << "Encountered error: " << status;           \
    ++error_count;                                        \
  }

RecordsWithErrorCount<pgsql::Record> StitchFrames(std::deque<pgsql::RegularMessage>* reqs,
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
    auto& req = *req_iter;
    ++req_iter;

    // TODO(yzhao): Use a map to encode request type and the actions to find the response.
    // So we can get rid of the switch statement.
    switch (req.tag) {
      case Tag::kReadyForQuery:
      case Tag::kSync:
      case Tag::kCopyFail:
      case Tag::kClose:
      case Tag::kPasswd:
        VLOG(1) << "Ignore tag: " << static_cast<char>(req.tag);
        break;
      case Tag::kQuery: {
        CALL_HANDLER(QueryReqResp, HandleQuery(req, &resp_iter, resps->end(), &req_resp));
        break;
      }
      case Tag::kParse: {
        CALL_HANDLER(ParseReqResp, HandleParse(req, &resp_iter, resps->end(), &req_resp, state));
        break;
      }
      case Tag::kBind: {
        CALL_HANDLER(BindReqResp, HandleBind(req, &resp_iter, resps->end(), &req_resp, state));
        break;
      }
      case Tag::kDesc: {
        CALL_HANDLER(DescReqResp, HandleDesc(req, &resp_iter, resps->end(), &req_resp));
        break;
      }
      case Tag::kExecute: {
        CALL_HANDLER(ExecReqResp, HandleExecute(req, &resp_iter, resps->end(), &req_resp, state));
        break;
      }
      case Tag::kTerminate: {
        // TODO(oazizi): Add a record here.
        break;
      }
      default:
        LOG_EVERY_N(WARNING, 100) << "Unhandled or invalid tag: " << static_cast<char>(req.tag);
        break;
    }
  }

  auto it = reqs->begin();
  while (it != reqs->end()) {
    if (!(*it).consumed) {
      break;
    }
    it++;
  }

  // Since postgres is an in-order protocol, remove the consumed requests and
  // all responses. We assume if a response is accessible, the matching request
  // would have been seen already.
  // TODO(ddelnano): Standarize this across protocols at a later time. See
  // https://github.com/pixie-io/pixie/issues/733 for more details.
  reqs->erase(reqs->begin(), it);
  resps->clear();

  return {records, error_count};
}

}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
