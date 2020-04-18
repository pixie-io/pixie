#pragma once

#include <deque>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/common/event_parser.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/common/stitcher.h"
#include "src/stirling/pgsql/types.h"

namespace pl {
namespace stirling {

namespace pgsql {

/**
 * Parse input data into messages.
 */
ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg);

ParseState ParseStartupMessage(std::string_view* buf, StartupMessage* msg);

std::vector<std::string_view> ParseRowDesc(std::string_view row_desc);
std::vector<std::optional<std::string_view>> ParseDataRow(std::string_view data_row);

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps);

using MsgDeqIter = std::deque<RegularMessage>::iterator;

/**
 * Return a formatted string for messages that can form the response for a query.
 * The input result argument begin is modified to point to the next message that has not been
 * examined yet.
 */
StatusOr<RegularMessage> AssembleQueryResp(MsgDeqIter* begin, const MsgDeqIter& end);

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<RegularMessage>* reqs,
                                                   std::deque<RegularMessage>* resps);

}  // namespace pgsql

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, pgsql::RegularMessage* frame);

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(MessageType type, std::string_view buf,
                                                size_t start);

// TODO(yzhao): Move into stitcher.h.
RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps,
                                                   NoState* /*state*/);

}  // namespace stirling
}  // namespace pl
