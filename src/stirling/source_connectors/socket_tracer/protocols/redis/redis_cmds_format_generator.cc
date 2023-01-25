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

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include "src/common/base/base.h"

DEFINE_string(redis_cmds, "", "A text file lists all Redis command names on each line.");
DEFINE_string(redis_cmdargs, "",
              "A text file lists the names and descriptions of all Redis commands on each line.");

using ::px::ReadFileToString;
using ::px::Status;
using ::px::VectorView;

// Returns a list of string lists. Each string list contains command name, 0 or more command
// arguments descriptions.
Status Main(std::string_view redis_cmds, std::string_view redis_cmdargs,
            std::vector<std::vector<std::string_view>>* commands) {
  absl::flat_hash_set<std::string_view> redis_cmd_set =
      absl::StrSplit(redis_cmds, "\n", absl::SkipWhitespace());

  std::vector<std::string_view> lines = absl::StrSplit(redis_cmdargs, "\n", absl::SkipWhitespace());

  std::vector<std::string_view> output_line;

  for (std::string_view line : lines) {
    constexpr std::string_view kWhiteSpaces = " \t\v\n";
    absl::ConsumePrefix(&line, kWhiteSpaces);
    absl::ConsumeSuffix(&line, kWhiteSpaces);

    if (redis_cmd_set.contains(line) && !output_line.empty()) {
      commands->push_back(std::move(output_line));
    }
    output_line.push_back(line);
  }

  if (!output_line.empty()) {
    commands->push_back(std::move(output_line));
  }
  return Status::OK();
}

std::string FomatCommand(const std::vector<std::string_view>& cmd_and_arg_descs) {
  std::vector<std::string> args;

  for (const auto c : cmd_and_arg_descs) {
    if (absl::StrContains(c, "\"")) {
      args.push_back(absl::Substitute("R\"($0)\"", c));
    } else {
      args.push_back(absl::Substitute(R"("$0")", c));
    }
  }

  std::string args_string = absl::Substitute(R"({$0})", absl::StrJoin(args, ", "));

  return absl::Substitute(R"({"$0", $1},)", cmd_and_arg_descs.front(), args_string);
}

// Prints a map from redis command name to its arguments names.
int main(int argc, char* argv[]) {
  px::EnvironmentGuard env_guard(&argc, argv);

  CHECK(!FLAGS_redis_cmdargs.empty()) << "--redis_cmdargs must be specified.";
  CHECK(!FLAGS_redis_cmds.empty()) << "--redis_cmds must be specified.";

  PX_ASSIGN_OR_EXIT(std::string redis_cmdargs, ReadFileToString(FLAGS_redis_cmdargs));
  PX_ASSIGN_OR_EXIT(std::string redis_cmds, ReadFileToString(FLAGS_redis_cmds));

  std::vector<std::vector<std::string_view>> commands;

  PX_CHECK_OK(Main(redis_cmds, redis_cmdargs, &commands));

  for (const auto& command : commands) {
    std::cout << FomatCommand(command) << std::endl;
  }

  return 0;
}
