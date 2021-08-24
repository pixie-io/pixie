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

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {
namespace mysql {

inline std::string CommandName(int cmd) {
  switch (cmd) {
    case 0x00:
      return "Sleep";
    case 0x01:
      return "Quit";
    case 0x02:
      return "InitDB";
    case 0x03:
      return "Query";
    case 0x04:
      return "FieldList";
    case 0x05:
      return "CreateDB";
    case 0x06:
      return "DropDB";
    case 0x07:
      return "Refresh";
    case 0x08:
      return "Shutdown";
    case 0x09:
      return "Statistics";
    case 0x0a:
      return "ProcessInfo";
    case 0x0b:
      return "Connect";
    case 0x0c:
      return "ProcessKill";
    case 0x0d:
      return "Debug";
    case 0x0e:
      return "Ping";
    case 0x0f:
      return "Time";
    case 0x10:
      return "DelayedInsert";
    case 0x11:
      return "ChangeUser";
    case 0x12:
      return "BinlogDump";
    case 0x13:
      return "TableDump";
    case 0x14:
      return "ConnectOut";
    case 0x15:
      return "RegisterSlave";
    case 0x16:
      return "StmtPrepare";
    case 0x17:
      return "StmtExecute";
    case 0x18:
      return "StmtSendLongData";
    case 0x19:
      return "StmtClose";
    case 0x1a:
      return "StmtReset";
    case 0x1b:
      return "SetOption";
    case 0x1c:
      return "StmtFetch";
    case 0x1d:
      return "Daemon";
    case 0x1e:
      return "BinlogDumpGTID";
    case 0x1f:
      return "ResetConnection";
    default:
      return std::to_string(cmd);
  }
}

}  // namespace mysql

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
