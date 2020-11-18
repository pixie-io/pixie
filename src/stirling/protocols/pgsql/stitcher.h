#pragma once

#include <deque>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/protocols/common/stitcher.h"
#include "src/stirling/protocols/pgsql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace pgsql {

/**
 * Handle*() functions accept one request message and a list response messages; and find the
 * relevant response messages for the request message (by looking for the tags specified by the
 * PGSQL wire protocol), and writes the messages in the input result argument.
 */
Status HandleQuery(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                   QueryReqResp* req_resp);
Status FillQueryResp(MsgDeqIter* resp_iter, const MsgDeqIter& end, QueryReqResp::QueryResp* resp);
Status HandleParse(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                   ParseReqResp* req_resp, State* state);
Status FillStmtDescResp(MsgDeqIter* resp_iter, const MsgDeqIter& end, DescReqResp::Resp* req_resp);
Status FillPortalDescResp(MsgDeqIter* resp_iter, const MsgDeqIter& end,
                          DescReqResp::Resp* req_resp);
Status HandleDesc(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                  DescReqResp* req_resp);
Status HandleBind(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                  BindReqResp* req_resp, State* state);
Status HandleExecute(const RegularMessage& msg, MsgDeqIter* resp_iter, const MsgDeqIter& end,
                     ExecReqResp* req_resp, State* state);

RecordsWithErrorCount<Record> StitchFrames(std::deque<RegularMessage>* reqs,
                                           std::deque<RegularMessage>* resps, State* state);

}  // namespace pgsql

template <>
inline RecordsWithErrorCount<pgsql::Record> StitchFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                         std::deque<pgsql::RegularMessage>* resps,
                                                         pgsql::StateWrapper* state) {
  return pgsql::StitchFrames(reqs, resps, &state->global);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
