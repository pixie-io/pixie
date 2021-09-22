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

#pragma once

#include <deque>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

namespace px {
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
ParseState ParseFrame(message_type_t type, std::string_view* buf, pgsql::RegularMessage* frame,
                      pgsql::StateWrapper* /*state*/);

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(message_type_t type, std::string_view buf,
                                                size_t start, pgsql::StateWrapper* /*state*/);

}  // namespace protocols
}  // namespace stirling
}  // namespace px
