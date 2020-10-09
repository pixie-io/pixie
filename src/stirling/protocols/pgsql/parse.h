#pragma once

#include <deque>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/bcc_bpf_interface/common.h"
#include "src/stirling/common/parse_state.h"
#include "src/stirling/protocols/common/event_parser.h"
#include "src/stirling/protocols/pgsql/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace pgsql {

/**
 * Parse input data into messages.
 */
ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg);

Status ParseStartupMessage(std::string_view* buf, StartupMessage* msg);
Status ParseCmdCmpl(const RegularMessage& msg, CmdCmpl* cmd_cmpl);
Status ParseDataRow(const RegularMessage& msg, DataRow* data_row);
Status ParseBindRequest(const RegularMessage& msg, BindRequest* res);
Status ParseParamDesc(const RegularMessage& msg, ParamDesc* param_desc);
// This is for 'Parse' message.
Status ParseParse(const RegularMessage& msg, Parse* parse);
Status ParseRowDesc(const RegularMessage& msg, RowDesc* row_desc);
Status ParseErrResp(const RegularMessage& msg, ErrResp* err_resp);
Status ParseDesc(const RegularMessage& msg, Desc* desc);

size_t FindFrameBoundary(std::string_view buf, size_t start);

}  // namespace pgsql

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, pgsql::RegularMessage* frame);

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(MessageType type, std::string_view buf,
                                                size_t start);

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
