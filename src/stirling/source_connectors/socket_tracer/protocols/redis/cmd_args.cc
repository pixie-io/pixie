#include "src/stirling/source_connectors/socket_tracer/protocols/redis/cmd_args.h"

#include "src/common/json/json.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

using ::pl::utils::JSONObjectBuilder;
using ::pl::utils::ToJSONString;

constexpr std::string_view kListArgSuffix = " ...]";
constexpr std::string_view kEvalSHA = "EVALSHA";
constexpr std::string_view kSet = "SET";
constexpr std::string_view kSScan = "SSCAN";

// Returns true if all characters are one of a-z & 0-9.
bool IsLowerAlphaNum(std::string_view name) {
  for (char c : name) {
    if (!std::islower(c) && !std::isdigit(c)) {
      return false;
    }
  }
  return true;
}

// An argument name is composed of all lower-case letters.
bool IsFixedArg(std::string_view arg_name) {
  if (!IsLowerAlphaNum(arg_name)) {
    return false;
  }
  return true;
}

std::string_view GetListArgName(std::string_view arg_desc) {
  return arg_desc.substr(0, arg_desc.find_first_of(' '));
}

// An optional list argument is described as "<name> [<name> ...]".
bool IsListArg(std::string_view arg_desc) {
  if (!absl::EndsWith(arg_desc, kListArgSuffix)) {
    return false;
  }
  if (!IsLowerAlphaNum(GetListArgName(arg_desc))) {
    return false;
  }
  return true;
}

std::string_view GetOptArgName(std::string_view arg_desc) {
  arg_desc.remove_prefix(1);
  arg_desc.remove_suffix(1);
  return arg_desc;
}

bool IsOptArg(std::string_view arg_desc) {
  if (arg_desc.front() != '[' || arg_desc.back() != ']') {
    return false;
  }
  if (!IsLowerAlphaNum(GetOptArgName(arg_desc))) {
    return false;
  }
  return true;
}

// Detects the arguments format of the input argument names specification.
// See https://redis.io/commands
StatusOr<std::vector<ArgDesc>> ParseArgDescs(const std::vector<std::string_view>& arg_descs) {
  std::vector<ArgDesc> args;
  for (auto arg_desc : arg_descs) {
    if (IsFixedArg(arg_desc)) {
      args.push_back({arg_desc, Format::kFixed});
    } else if (IsListArg(arg_desc)) {
      args.push_back({GetListArgName(arg_desc), Format::kList});
    } else if (IsOptArg(arg_desc)) {
      args.push_back({GetOptArgName(arg_desc), Format::kOpt});
    } else {
      return error::InvalidArgument("Invalid arguments format: $0", absl::StrJoin(arg_descs, " "));
    }
  }
  return args;
}

// Extracts arguments from the input argument values, and formats them according to the argument
// format.
Status FmtArg(const ArgDesc& arg_desc, VectorView<std::string>* args,
              JSONObjectBuilder* json_builder) {
#define RETURN_ERROR_IF_EMPTY(arg_values, arg_desc)                               \
  if (arg_values->empty()) {                                                      \
    return error::InvalidArgument("No values for argument: $0:$1", arg_desc.name, \
                                  magic_enum::enum_name(arg_desc.format));        \
  }
  switch (arg_desc.format) {
    case Format::kFixed:
      RETURN_ERROR_IF_EMPTY(args, arg_desc);
      json_builder->WriteKV(arg_desc.name, args->front());
      args->pop_front();
      break;
    case Format::kList:
      RETURN_ERROR_IF_EMPTY(args, arg_desc);
      json_builder->WriteKV(arg_desc.name, *args);
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

  JSONObjectBuilder json_builder;

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
  JSONObjectBuilder builder;
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
  JSONObjectBuilder builder;
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

}  // namespace

CmdArgs::CmdArgs(std::initializer_list<const char*> cmd_args) {
  cmd_name_ = *cmd_args.begin();
  cmd_args_.insert(cmd_args_.end(), cmd_args.begin() + 1, cmd_args.end());
  // Uses ToJSONString() to produce [], instead of JSONObjectBuilder, which produces {} for empty
  // list.
  if (!cmd_args_.empty()) {
    auto cmd_args_or = ParseArgDescs(cmd_args_);
    if (cmd_args_or.ok()) {
      cmd_arg_descs_ = cmd_args_or.ConsumeValueOrDie();
    }
  }
}

StatusOr<std::string> CmdArgs::FmtArgs(VectorView<std::string> args) const {
  if (cmd_name_ == kEvalSHA) {
    auto res_or = FormatEvalSHAArgs(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (cmd_name_ == kSet) {
    auto res_or = FormatSet(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (cmd_name_ == kSScan) {
    auto res_or = FormatSScan(args);
    if (res_or.ok()) {
      return res_or.ConsumeValueOrDie();
    }
  }
  if (!cmd_arg_descs_.has_value()) {
    return error::ResourceUnavailable(
        "Unable to format arguments, because Redis command argument "
        "descriptions were not specified.");
  }
  JSONObjectBuilder json_builder;
  for (const auto& arg : cmd_arg_descs_.value()) {
    PL_RETURN_IF_ERROR(FmtArg(arg, &args, &json_builder));
  }
  return json_builder.GetString();
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
