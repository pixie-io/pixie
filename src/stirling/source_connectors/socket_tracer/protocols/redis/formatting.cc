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

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/formatting.h"

#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/cmd_args.h"

namespace px {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

constexpr std::string_view kEvalSHA = "EVALSHA";
constexpr std::string_view kSet = "SET";
constexpr std::string_view kSScan = "SSCAN";

// Returns a JSON string that formats the input arguments as a JSON array.
std::string FormatAsJSONArray(VectorView<std::string> args) {
  std::vector<std::string_view> args_copy = {args.begin(), args.end()};
  return utils::ToJSONString(args_copy);
}

// EVALSHA executes a previous cached script on Redis server:
//
// SCRIPT LOAD "return 1"
// e0e1f9fabfc9d4800c877a703b823ac0578ff8db // sha hash, used in EVALSHA to reference this script.
// EVALSHA e0e1f9fabfc9d4800c877a703b823ac0578ff8db 2 1 1 2 2
StatusOr<std::string> FormatEvalSHAArgs(VectorView<std::string> args) {
  constexpr size_t kEvalSHAMinArgCount = 4;
  if (args.size() < kEvalSHAMinArgCount) {
    return error::InvalidArgument("EVALSHA requires at least 4 arguments, got $0",
                                  absl::StrJoin(args, ", "));
  }
  if (args.size() % 2 != 0) {
    return error::InvalidArgument("EVALSHA requires even number of arguments, got $0",
                                  absl::StrJoin(args, ", "));
  }

  utils::JSONObjectBuilder json_builder;

  json_builder.WriteKV("sha1", args[0]);
  json_builder.WriteKV("numkeys", args[1]);

  // The first 2 arguments are consumed.
  args.pop_front(2);

  // The rest of the values are divided equally to the rest arguments.
  auto args_copy = args;
  args_copy.pop_back(args_copy.size() / 2);
  json_builder.WriteKV("key", args_copy);

  args.pop_front(args.size() / 2);
  json_builder.WriteKV("value", args);

  return json_builder.GetString();
}

// SET is formatted as:
// SET key value [EX seconds|PX milliseconds|EXAT timestamp|PXAT milliseconds-timestamp|KEEPTTL]
// [NX|XX] [GET]
//
// The values after key & value is grouped into options field.
StatusOr<std::string> FormatSet(VectorView<std::string> args) {
  constexpr size_t kMinArgsCount = 2;
  if (args.size() < kMinArgsCount) {
    return error::InvalidArgument("SET expects at least 2 arguments, got $0", args.size());
  }
  utils::JSONObjectBuilder builder;
  builder.WriteKV("key", args[0]);
  builder.WriteKV("value", args[1]);
  args.pop_front(2);

  constexpr std::string_view kExpireSecondsToken = "EX";
  constexpr std::string_view kExpireMillisToken = "PX";
  constexpr std::string_view kExpireAtSecondsToken = "EXAT";
  constexpr std::string_view kExpireAtMillisToken = "PXAT";

  std::vector<std::string> opts;

  for (size_t i = 0; i < args.size(); ++i) {
    std::string arg_upper = absl::AsciiStrToUpper(args[i]);

    if (arg_upper == kExpireSecondsToken || arg_upper == kExpireMillisToken ||
        arg_upper == kExpireAtSecondsToken || arg_upper == kExpireAtMillisToken) {
      if (i + 1 >= args.size()) {
        return error::InvalidArgument("Invalid format, expect argument after $0, got nothing.",
                                      args[i]);
      }
      opts.push_back(absl::StrCat(args[i], " ", args[i + 1]));
      // Skip the next argument.
      ++i;
    } else {
      opts.push_back(args[i]);
    }
  }

  builder.WriteKV("options", opts);

  return builder.GetString();
}

// SSCAN is formatted as:
// SSCAN key cursor [MATCH pattern] [COUNT count]
StatusOr<std::string> FormatSScan(VectorView<std::string> args) {
  constexpr size_t kMinArgsCount = 2;
  if (args.size() < kMinArgsCount) {
    return error::InvalidArgument("Redis SSCAN command expects at least 2 arguments, got $0",
                                  args.size());
  }
  utils::JSONObjectBuilder builder;
  builder.WriteKV("key", args[0]);
  builder.WriteKV("cursor", args[1]);
  args.pop_front(2);

  constexpr std::string_view kMatchToken = "MATCH";
  constexpr std::string_view kCountToken = "COUNT";

  std::vector<std::string> opts;

  for (size_t i = 0; i < args.size(); ++i) {
    std::string arg_upper = absl::AsciiStrToUpper(args[i]);
    if (i + 1 >= args.size()) {
      return error::InvalidArgument("Invalid format, expect argument after $0, got nothing.",
                                    args[i]);
    }
    if (arg_upper == kMatchToken) {
      builder.WriteKV("pattern", args[i + 1]);
      ++i;
    } else if (arg_upper == kCountToken) {
      builder.WriteKV("count", args[i + 1]);
      ++i;
    } else {
      return error::InvalidArgument(
          "Invalid Redis SSCAN command arguments format, "
          "expect MATCH or COUNT, got $0",
          args[i]);
    }
  }

  return builder.GetString();
}

// Extracts arguments from the input argument values, and formats them according to the argument
// format.
Status FmtArg(const ArgDesc& arg_desc, VectorView<std::string>* args,
              utils::JSONObjectBuilder* json_builder) {
#define RETURN_ERROR_IF_EMPTY(arg_values, arg_desc)                                   \
  if (arg_values->empty()) {                                                          \
    return error::InvalidArgument("No values for argument: $0", arg_desc.ToString()); \
  }
  switch (arg_desc.format) {
    case Format::kFixed:
      RETURN_ERROR_IF_EMPTY(args, arg_desc);
      json_builder->WriteKV(arg_desc.name, args->front());
      args->pop_front();
      break;
    case Format::kList:
      RETURN_ERROR_IF_EMPTY(args, arg_desc);
      if (arg_desc.sub_fields.size() == 1) {
        json_builder->WriteKV(arg_desc.name, *args);
      } else if (args->size() % arg_desc.sub_fields.size() == 0) {
        json_builder->WriteRepeatedKVs(arg_desc.name, arg_desc.sub_fields, *args);
      } else {
        return error::InvalidArgument("Invalid number of argument values");
      }
      // Consume all the rest of the argument values.
      args->clear();
      break;
    case Format::kOpt:
      if (!args->empty()) {
        json_builder->WriteKV(arg_desc.name, args->front());
        args->pop_front();
      }
      break;
  }
#undef RETURN_ERROR_IF_EMPTY
  return Status::OK();
}

// Formats the input argument value based on this detected format of this command.
StatusOr<std::string> FmtArgs(const CmdArgs& cmd_args, VectorView<std::string> args) {
  if (cmd_args.cmd_name_ == kEvalSHA) {
    auto res_or = FormatEvalSHAArgs(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (cmd_args.cmd_name_ == kSet) {
    auto res_or = FormatSet(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (cmd_args.cmd_name_ == kSScan) {
    auto res_or = FormatSScan(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (!cmd_args.cmd_arg_descs_.has_value()) {
    return error::ResourceUnavailable(
        "Unable to format arguments, because Redis command argument "
        "descriptions were not specified.");
  }
  utils::JSONObjectBuilder json_builder;
  for (const auto& arg : cmd_args.cmd_arg_descs_.value()) {
    PX_RETURN_IF_ERROR(FmtArg(arg, &args, &json_builder));
  }
  return json_builder.GetString();
}

}  // namespace

// Redis wire protocol said requests are array consisting of bulk strings:
// https://redis.io/topics/protocol#sending-commands-to-a-redis-server
void FormatArrayMessage(VectorView<std::string> payloads_view, Message* msg) {
  std::optional<const CmdArgs*> cmd_args_opt = GetCmdAndArgs(&payloads_view);

  // If no command is found, this array message is formatted as JSON array.
  if (!cmd_args_opt.has_value()) {
    msg->payload = FormatAsJSONArray(payloads_view);
    return;
  }

  msg->command = cmd_args_opt.value()->cmd_name_;

  // FmtArgs might fail for requests with invalid format, for example, incorrect number of
  // arguments; which happens rarely.
  auto payload_or = FmtArgs(*cmd_args_opt.value(), payloads_view);
  if (payload_or.ok()) {
    msg->payload = payload_or.ConsumeValueOrDie();
  } else {
    msg->payload = FormatAsJSONArray(payloads_view);
  }
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace px
