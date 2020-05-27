#pragma once

#include <deque>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/common/event_parser.h"
#include "src/stirling/common/parse_state.h"
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
ParseState ParseBindRequest(const RegularMessage& msg, BindRequest* res);
ParseState ParseParamDesc(std::string_view payload, ParamDesc* param_desc);
// This is for 'Parse' message.
Status ParseParse(const RegularMessage& msg, Parse* parse);
ParseState ParseRowDesc(std::string_view payload, RowDesc* row_desc);
Status ParseErrResp(std::string_view payload, ErrResp* err_resp);

size_t FindFrameBoundary(std::string_view buf, size_t start);

}  // namespace pgsql

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, pgsql::RegularMessage* frame);

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(MessageType type, std::string_view buf,
                                                size_t start);

}  // namespace stirling
}  // namespace pl
