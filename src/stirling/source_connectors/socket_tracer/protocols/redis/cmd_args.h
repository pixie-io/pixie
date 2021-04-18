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

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace protocols {
namespace redis {

// Describes the format of a single Redis command argument.
enum class Format {
  // This argument value must be specified, therefore the simplest to format.
  kFixed,

  // This argument value has 0 or more elements.
  kList,

  // This argument value can be omitted.
  kOpt,
};

// Describes a single argument.
struct ArgDesc {
  std::string_view name;
  std::vector<std::string_view> sub_fields;
  Format format;

  std::string ToString() const {
    return absl::Substitute("[$0[$1]::$2]", name.front(), absl::StrJoin(sub_fields, ","),
                            magic_enum::enum_name(format));
  }
};

// Describes the arguments of a Redis command.
struct CmdArgs {
  // Allows convenient initialization of kCmdList below.
  //
  // TODO(yzhao): Let the :redis_cmds_format_generator and gen_redis_cmds.sh to statically produces
  // the initialization code for kCmdList that fills in the command argument format as well.
  CmdArgs(std::initializer_list<const char*> cmd_args);

  // Cannot move this out of class definition. Doing that gcc build fails because this is not used.
  std::string ToString() const {
    std::string cmd_arg_descs_str = "<null>";
    if (cmd_arg_descs_.has_value()) {
      cmd_arg_descs_str = absl::StrJoin(cmd_arg_descs_.value(), ", ",
                                        [](std::string* buf, const ArgDesc& arg_desc) {
                                          absl::StrAppend(buf, arg_desc.ToString());
                                        });
    }
    return absl::Substitute("name: $0 cmd_args: $1 formats: $2", cmd_name_,
                            absl::StrJoin(cmd_args_, ", "), cmd_arg_descs_str);
  }

  std::string_view cmd_name_;
  std::vector<std::string_view> cmd_args_;
  std::optional<std::vector<ArgDesc>> cmd_arg_descs_;
};

// Returns the object that describes the command of the payloads, if there is a matching one.
std::optional<const CmdArgs*> GetCmdAndArgs(VectorView<std::string>* payloads);

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace px
