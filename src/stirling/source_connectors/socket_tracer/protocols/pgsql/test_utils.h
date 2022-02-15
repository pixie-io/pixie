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

#include <string>

#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {
namespace testutils {

std::string RegularMessageToByteString(const RegularMessage& msg);
std::string CmdCmplToByteString(const CmdCmpl& cmd_cmpl);
std::string DataRowToByteString(const DataRow& data_row);
std::string RowDescToByteString(const RowDesc& row_desc);

std::string CmdCmplToPayload(const CmdCmpl& cmd_cmpl);
std::string DataRowToPayload(const DataRow& data_row);
std::string RowDescToPayload(const RowDesc& row_desc);

}  // namespace testutils
}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
