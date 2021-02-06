#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

using ::pl::utils::JSONObjectBuilder;
using ::pl::utils::ToJSONString;

constexpr char kSimpleStringMarker = '+';
constexpr char kErrorMarker = '-';
constexpr char kIntegerMarker = ':';
constexpr char kBulkStringsMarker = '$';
constexpr char kArrayMarker = '*';
// This is Redis' universal terminating sequence.
constexpr std::string_view kTerminalSequence = "\r\n";
constexpr int kNullSize = -1;
constexpr std::string_view kEvalSHA = "EVALSHA";

// Formats the input arguments as JSON array.
std::string FormatAsJSONArray(VectorView<std::string> args) {
  std::vector<std::string_view> args_copy = {args.begin(), args.end()};
  return ToJSONString(args_copy);
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

// Describes the arguments of a Redis command.
// TODO(yzhao): Move to a separate file.
class CmdArgs {
 public:
  // Allows convenient initialization of kCmdList below.
  //
  // TODO(yzhao): Let the :redis_cmds_format_generator and gen_redis_cmds.sh to statically produces
  // the initialization code for kCmdList that fills in the command argument format as well.
  CmdArgs(std::initializer_list<const char*> cmd_args) {
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

  // Formats the input argument value based on this detected format of this command.
  std::string FmtArgs(VectorView<std::string> args) const {
    if (cmd_name_ == kEvalSHA) {
      auto res_or = FormatEvalSHAArgs(args);
      if (res_or.ok()) {
        return res_or.ConsumeValueOrDie();
      }
    }
    if (!cmd_arg_descs_.has_value()) {
      return FormatAsJSONArray(args);
    }
    JSONObjectBuilder json_builder;
    for (const auto& arg : cmd_arg_descs_.value()) {
      if (!FmtArg(arg, &args, &json_builder).ok()) {
        return FormatAsJSONArray(args);
      }
    }
    return json_builder.GetString();
  }

  std::string ToString() const {
    std::string cmd_arg_descs_str = "<null>";
    if (cmd_arg_descs_.has_value()) {
      cmd_arg_descs_str = absl::StrJoin(cmd_arg_descs_.value(), ", ",
                                        [](std::string* buf, const ArgDesc& arg_desc) {
                                          absl::StrAppend(buf, arg_desc.ToString());
                                        });
    }
    return absl::Substitute("cmd_args: $0 formats: $1", absl::StrJoin(cmd_args_, ", "),
                            cmd_arg_descs_str);
  }

 private:
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
    Format format;

    std::string ToString() const {
      return absl::Substitute("[$0::$1]", name, magic_enum::enum_name(format));
    }
  };

  // An argument name is composed of all lower-case letters.
  static bool IsFixedArg(std::string_view arg_name) {
    if (!IsLowerAlphaNum(arg_name)) {
      return false;
    }
    return true;
  }

  static constexpr std::string_view kListArgSuffix = " ...]";

  // Returns true if all characters are one of a-z & 0-9.
  static bool IsLowerAlphaNum(std::string_view name) {
    for (char c : name) {
      if (!std::islower(c) && !std::isdigit(c)) {
        return false;
      }
    }
    return true;
  }

  // An optional list argument is described as "<name> [<name> ...]".
  static bool IsListArg(std::string_view arg_desc) {
    if (!absl::EndsWith(arg_desc, kListArgSuffix)) {
      return false;
    }
    if (!IsLowerAlphaNum(GetListArgName(arg_desc))) {
      return false;
    }
    return true;
  }

  static std::string_view GetOptArgName(std::string_view arg_desc) {
    arg_desc.remove_prefix(1);
    arg_desc.remove_suffix(1);
    return arg_desc;
  }

  static bool IsOptArg(std::string_view arg_desc) {
    if (arg_desc.front() != '[' || arg_desc.back() != ']') {
      return false;
    }
    if (!IsLowerAlphaNum(GetOptArgName(arg_desc))) {
      return false;
    }
    return true;
  }

  static std::string_view GetListArgName(std::string_view arg_desc) {
    return arg_desc.substr(0, arg_desc.find_first_of(' '));
  }

  // Detects the arguments format of the input argument names specification.
  // See https://redis.io/commands
  static StatusOr<std::vector<ArgDesc>> ParseArgDescs(
      const std::vector<std::string_view>& arg_descs) {
    std::vector<ArgDesc> args;
    for (auto arg_desc : arg_descs) {
      if (IsFixedArg(arg_desc)) {
        args.push_back({arg_desc, Format::kFixed});
      } else if (IsListArg(arg_desc)) {
        args.push_back({GetListArgName(arg_desc), Format::kList});
      } else if (IsOptArg(arg_desc)) {
        args.push_back({GetOptArgName(arg_desc), Format::kOpt});
      } else {
        return error::InvalidArgument("Invalid arguments format: $0",
                                      absl::StrJoin(arg_descs, " "));
      }
    }
    return args;
  }

  // Extracts arguments from the input argument values, and formats them according to the argument
  // format.
  static Status FmtArg(const ArgDesc& arg_desc, VectorView<std::string>* args,
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

  std::string_view cmd_name_;
  std::vector<std::string_view> cmd_args_;
  std::optional<std::vector<ArgDesc>> cmd_arg_descs_;
};

// This list is produced with by:
//   //src/stirling/source_connectors/socket_tracer/protocols/redis:redis_cmds_format_generator
//
//   Read its help message to get the instructions.
const absl::flat_hash_map<std::string_view, CmdArgs> kCmdList = {
    {"ACL LOAD", {"ACL LOAD"}},
    {"ACL SAVE", {"ACL SAVE"}},
    {"ACL LIST", {"ACL LIST"}},
    {"ACL USERS", {"ACL USERS"}},
    {"ACL GETUSER", {"ACL GETUSER", "username"}},
    {"ACL SETUSER", {"ACL SETUSER", "username", "[rule [rule ...]]"}},
    {"ACL DELUSER", {"ACL DELUSER", "username [username ...]"}},
    {"ACL CAT", {"ACL CAT", "[categoryname]"}},
    {"ACL GENPASS", {"ACL GENPASS", "[bits]"}},
    {"ACL WHOAMI", {"ACL WHOAMI"}},
    {"ACL LOG", {"ACL LOG", "[count or RESET]"}},
    {"ACL HELP", {"ACL HELP"}},
    {"APPEND", {"APPEND", "key", "value"}},
    {"AUTH", {"AUTH", "[username]", "password"}},
    {"BGREWRITEAOF", {"BGREWRITEAOF"}},
    {"BGSAVE", {"BGSAVE", "[SCHEDULE]"}},
    {"BITCOUNT", {"BITCOUNT", "key", "[start end]"}},
    {"BITFIELD",
     {"BITFIELD", "key", "[GET type offset]", "[SET type offset value]",
      "[INCRBY type offset increment]", "[OVERFLOW WRAP|SAT|FAIL]"}},
    {"BITOP", {"BITOP", "operation", "destkey", "key [key ...]"}},
    {"BITPOS", {"BITPOS", "key", "bit", "[start]", "[end]"}},
    {"BLPOP", {"BLPOP", "key [key ...]", "timeout"}},
    {"BRPOP", {"BRPOP", "key [key ...]", "timeout"}},
    {"BRPOPLPUSH", {"BRPOPLPUSH", "source", "destination", "timeout"}},
    {"BLMOVE", {"BLMOVE", "source", "destination", "LEFT|RIGHT", "LEFT|RIGHT", "timeout"}},
    {"BZPOPMIN", {"BZPOPMIN", "key [key ...]", "timeout"}},
    {"BZPOPMAX", {"BZPOPMAX", "key [key ...]", "timeout"}},
    {"CLIENT CACHING", {"CLIENT CACHING", "YES|NO"}},
    {"CLIENT ID", {"CLIENT ID"}},
    {"CLIENT INFO", {"CLIENT INFO"}},
    {"CLIENT KILL",
     {"CLIENT KILL", "[ip:port]", "[ID client-id]", "[TYPE normal|master|slave|pubsub]",
      "[USER username]", "[ADDR ip:port]", "[SKIPME yes/no]"}},
    {"CLIENT LIST",
     {"CLIENT LIST", "[TYPE normal|master|replica|pubsub]", "[ID client-id [client-id ...]]"}},
    {"CLIENT GETNAME", {"CLIENT GETNAME"}},
    {"CLIENT GETREDIR", {"CLIENT GETREDIR"}},
    {"CLIENT UNPAUSE", {"CLIENT UNPAUSE"}},
    {"CLIENT PAUSE", {"CLIENT PAUSE", "timeout", "[WRITE|ALL]"}},
    {"CLIENT REPLY", {"CLIENT REPLY", "ON|OFF|SKIP"}},
    {"CLIENT SETNAME", {"CLIENT SETNAME", "connection-name"}},
    {"CLIENT TRACKING",
     {"CLIENT TRACKING", "ON|OFF", "[REDIRECT client-id]", "[PREFIX prefix [PREFIX prefix ...]]",
      "[BCAST]", "[OPTIN]", "[OPTOUT]", "[NOLOOP]"}},
    {"CLIENT TRACKINGINFO", {"CLIENT TRACKINGINFO"}},
    {"CLIENT UNBLOCK", {"CLIENT UNBLOCK", "client-id", "[TIMEOUT|ERROR]"}},
    {"CLUSTER ADDSLOTS", {"CLUSTER ADDSLOTS", "slot [slot ...]"}},
    {"CLUSTER BUMPEPOCH", {"CLUSTER BUMPEPOCH"}},
    {"CLUSTER COUNT-FAILURE-REPORTS", {"CLUSTER COUNT-FAILURE-REPORTS", "node-id"}},
    {"CLUSTER COUNTKEYSINSLOT", {"CLUSTER COUNTKEYSINSLOT", "slot"}},
    {"CLUSTER DELSLOTS", {"CLUSTER DELSLOTS", "slot [slot ...]"}},
    {"CLUSTER FAILOVER", {"CLUSTER FAILOVER", "[FORCE|TAKEOVER]"}},
    {"CLUSTER FLUSHSLOTS", {"CLUSTER FLUSHSLOTS"}},
    {"CLUSTER FORGET", {"CLUSTER FORGET", "node-id"}},
    {"CLUSTER GETKEYSINSLOT", {"CLUSTER GETKEYSINSLOT", "slot", "count"}},
    {"CLUSTER INFO", {"CLUSTER INFO"}},
    {"CLUSTER KEYSLOT", {"CLUSTER KEYSLOT", "key"}},
    {"CLUSTER MEET", {"CLUSTER MEET", "ip", "port"}},
    {"CLUSTER MYID", {"CLUSTER MYID"}},
    {"CLUSTER NODES", {"CLUSTER NODES"}},
    {"CLUSTER REPLICATE", {"CLUSTER REPLICATE", "node-id"}},
    {"CLUSTER RESET", {"CLUSTER RESET", "[HARD|SOFT]"}},
    {"CLUSTER SAVECONFIG", {"CLUSTER SAVECONFIG"}},
    {"CLUSTER SET-CONFIG-EPOCH", {"CLUSTER SET-CONFIG-EPOCH", "config-epoch"}},
    {"CLUSTER SETSLOT",
     {"CLUSTER SETSLOT", "slot", "IMPORTING|MIGRATING|STABLE|NODE", "[node-id]"}},
    {"CLUSTER SLAVES", {"CLUSTER SLAVES", "node-id"}},
    {"CLUSTER REPLICAS", {"CLUSTER REPLICAS", "node-id"}},
    {"CLUSTER SLOTS", {"CLUSTER SLOTS"}},
    {"COMMAND", {"COMMAND"}},
    {"COMMAND COUNT", {"COMMAND COUNT"}},
    {"COMMAND GETKEYS", {"COMMAND GETKEYS"}},
    {"COMMAND INFO", {"COMMAND INFO", "command-name [command-name ...]"}},
    {"CONFIG GET", {"CONFIG GET", "parameter"}},
    {"CONFIG REWRITE", {"CONFIG REWRITE"}},
    {"CONFIG SET", {"CONFIG SET", "parameter", "value"}},
    {"CONFIG RESETSTAT", {"CONFIG RESETSTAT"}},
    {"COPY", {"COPY", "source", "destination", "[DB destination-db]", "[REPLACE]"}},
    {"DBSIZE", {"DBSIZE"}},
    {"DEBUG OBJECT", {"DEBUG OBJECT", "key"}},
    {"DEBUG SEGFAULT", {"DEBUG SEGFAULT"}},
    {"DECR", {"DECR", "key"}},
    {"DECRBY", {"DECRBY", "key", "decrement"}},
    {"DEL", {"DEL", "key [key ...]"}},
    {"DISCARD", {"DISCARD"}},
    {"DUMP", {"DUMP", "key"}},
    {"ECHO", {"ECHO", "message"}},
    {"EVAL", {"EVAL", "script", "numkeys", "key [key ...]", "arg [arg ...]"}},
    {"EVALSHA", {"EVALSHA", "sha1", "numkeys", "key [key ...]", "arg [arg ...]"}},
    {"EXEC", {"EXEC"}},
    {"EXISTS", {"EXISTS", "key [key ...]"}},
    {"EXPIRE", {"EXPIRE", "key", "seconds"}},
    {"EXPIREAT", {"EXPIREAT", "key", "timestamp"}},
    {"FAILOVER", {"FAILOVER", "[TO host port [FORCE]]", "[ABORT]", "[TIMEOUT milliseconds]"}},
    {"FLUSHALL", {"FLUSHALL", "[ASYNC|SYNC]"}},
    {"FLUSHDB", {"FLUSHDB", "[ASYNC|SYNC]"}},
    {"GEOADD",
     {"GEOADD", "key", "[NX|XX]", "[CH]",
      "longitude latitude member [longitude latitude member ...]"}},
    {"GEOHASH", {"GEOHASH", "key", "member [member ...]"}},
    {"GEOPOS", {"GEOPOS", "key", "member [member ...]"}},
    {"GEODIST", {"GEODIST", "key", "member1", "member2", "[m|km|ft|mi]"}},
    {"GEORADIUS",
     {"GEORADIUS", "key", "longitude", "latitude", "radius", "m|km|ft|mi", "[WITHCOORD]",
      "[WITHDIST]", "[WITHHASH]", "[COUNT count [ANY]]", "[ASC|DESC]", "[STORE key]",
      "[STOREDIST key]"}},
    {"GEORADIUSBYMEMBER",
     {"GEORADIUSBYMEMBER", "key", "member", "radius", "m|km|ft|mi", "[WITHCOORD]", "[WITHDIST]",
      "[WITHHASH]", "[COUNT count [ANY]]", "[ASC|DESC]", "[STORE key]", "[STOREDIST key]"}},
    {"GEOSEARCH",
     {"GEOSEARCH", "key", "[FROMMEMBER member]", "[FROMLONLAT longitude latitude]",
      "[BYRADIUS radius m|km|ft|mi]", "[BYBOX width height m|km|ft|mi]", "[ASC|DESC]",
      "[COUNT count [ANY]]", "[WITHCOORD]", "[WITHDIST]", "[WITHHASH]"}},
    {"GEOSEARCHSTORE",
     {"GEOSEARCHSTORE", "destination", "source", "[FROMMEMBER member]",
      "[FROMLONLAT longitude latitude]", "[BYRADIUS radius m|km|ft|mi]",
      "[BYBOX width height m|km|ft|mi]", "[ASC|DESC]", "[COUNT count [ANY]]", "[WITHCOORD]",
      "[WITHDIST]", "[WITHHASH]", "[STOREDIST]"}},
    {"GET", {"GET", "key"}},
    {"GETBIT", {"GETBIT", "key", "offset"}},
    {"GETDEL", {"GETDEL", "key"}},
    {"GETEX",
     {"GETEX", "key",
      "[EX seconds|PX milliseconds|EXAT timestamp|PXAT milliseconds-timestamp|PERSIST]"}},
    {"GETRANGE", {"GETRANGE", "key", "start", "end"}},
    {"GETSET", {"GETSET", "key", "value"}},
    {"HDEL", {"HDEL", "key", "field [field ...]"}},
    {"HELLO", {"HELLO", "[protover [AUTH username password] [SETNAME clientname]]"}},
    {"HEXISTS", {"HEXISTS", "key", "field"}},
    {"HGET", {"HGET", "key", "field"}},
    {"HGETALL", {"HGETALL", "key"}},
    {"HINCRBY", {"HINCRBY", "key", "field", "increment"}},
    {"HINCRBYFLOAT", {"HINCRBYFLOAT", "key", "field", "increment"}},
    {"HKEYS", {"HKEYS", "key"}},
    {"HLEN", {"HLEN", "key"}},
    {"HMGET", {"HMGET", "key", "field [field ...]"}},
    {"HMSET", {"HMSET", "key", "field value [field value ...]"}},
    {"HSET", {"HSET", "key", "field value [field value ...]"}},
    {"HSETNX", {"HSETNX", "key", "field", "value"}},
    {"HRANDFIELD", {"HRANDFIELD", "key", "[count [WITHVALUES]]"}},
    {"HSTRLEN", {"HSTRLEN", "key", "field"}},
    {"HVALS", {"HVALS", "key"}},
    {"INCR", {"INCR", "key"}},
    {"INCRBY", {"INCRBY", "key", "increment"}},
    {"INCRBYFLOAT", {"INCRBYFLOAT", "key", "increment"}},
    {"INFO", {"INFO", "[section]"}},
    {"LOLWUT", {"LOLWUT", "[VERSION version]"}},
    {"KEYS", {"KEYS", "pattern"}},
    {"LASTSAVE", {"LASTSAVE"}},
    {"LINDEX", {"LINDEX", "key", "index"}},
    {"LINSERT", {"LINSERT", "key", "BEFORE|AFTER", "pivot", "element"}},
    {"LLEN", {"LLEN", "key"}},
    {"LPOP", {"LPOP", "key", "[count]"}},
    {"LPOS", {"LPOS", "key", "element", "[RANK rank]", "[COUNT num-matches]", "[MAXLEN len]"}},
    {"LPUSH", {"LPUSH", "key", "element [element ...]"}},
    {"LPUSHX", {"LPUSHX", "key", "element [element ...]"}},
    {"LRANGE", {"LRANGE", "key", "start", "stop"}},
    {"LREM", {"LREM", "key", "count", "element"}},
    {"LSET", {"LSET", "key", "index", "element"}},
    {"LTRIM", {"LTRIM", "key", "start", "stop"}},
    {"MEMORY DOCTOR", {"MEMORY DOCTOR"}},
    {"MEMORY HELP", {"MEMORY HELP"}},
    {"MEMORY MALLOC-STATS", {"MEMORY MALLOC-STATS"}},
    {"MEMORY PURGE", {"MEMORY PURGE"}},
    {"MEMORY STATS", {"MEMORY STATS"}},
    {"MEMORY USAGE", {"MEMORY USAGE", "key", "[SAMPLES count]"}},
    {"MGET", {"MGET", "key [key ...]"}},
    {"MIGRATE",
     {"MIGRATE", "host", "port", R"(key|"")", "destination-db", "timeout", "[COPY]", "[REPLACE]",
      "[AUTH password]", "[AUTH2 username password]", "[KEYS key [key ...]]"}},
    {"MODULE LIST", {"MODULE LIST"}},
    {"MODULE LOAD", {"MODULE LOAD", "path", "[ arg [arg ...]]"}},
    {"MODULE UNLOAD", {"MODULE UNLOAD", "name"}},
    {"MONITOR", {"MONITOR"}},
    {"MOVE", {"MOVE", "key", "db"}},
    {"MSET", {"MSET", "key value [key value ...]"}},
    {"MSETNX", {"MSETNX", "key value [key value ...]"}},
    {"MULTI", {"MULTI"}},
    {"OBJECT", {"OBJECT", "subcommand", "[arguments [arguments ...]]"}},
    {"PERSIST", {"PERSIST", "key"}},
    {"PEXPIRE", {"PEXPIRE", "key", "milliseconds"}},
    {"PEXPIREAT", {"PEXPIREAT", "key", "milliseconds-timestamp"}},
    {"PFADD", {"PFADD", "key", "element [element ...]"}},
    {"PFCOUNT", {"PFCOUNT", "key [key ...]"}},
    {"PFMERGE", {"PFMERGE", "destkey", "sourcekey [sourcekey ...]"}},
    {"PING", {"PING", "[message]"}},
    {"PSETEX", {"PSETEX", "key", "milliseconds", "value"}},
    {"PSUBSCRIBE", {"PSUBSCRIBE", "pattern [pattern ...]"}},
    {"PUBSUB", {"PUBSUB", "subcommand", "[argument [argument ...]]"}},
    {"PTTL", {"PTTL", "key"}},
    {"PUBLISH", {"PUBLISH", "channel", "message"}},
    {"PUNSUBSCRIBE", {"PUNSUBSCRIBE", "[pattern [pattern ...]]"}},
    {"QUIT", {"QUIT"}},
    {"RANDOMKEY", {"RANDOMKEY"}},
    {"READONLY", {"READONLY"}},
    {"READWRITE", {"READWRITE"}},
    {"RENAME", {"RENAME", "key", "newkey"}},
    {"RENAMENX", {"RENAMENX", "key", "newkey"}},
    {"RESET", {"RESET"}},
    {"RESTORE",
     {"RESTORE", "key", "ttl", "serialized-value", "[REPLACE]", "[ABSTTL]", "[IDLETIME seconds]",
      "[FREQ frequency]"}},
    {"ROLE", {"ROLE"}},
    {"RPOP", {"RPOP", "key", "[count]"}},
    {"RPOPLPUSH", {"RPOPLPUSH", "source", "destination"}},
    {"LMOVE", {"LMOVE", "source", "destination", "LEFT|RIGHT", "LEFT|RIGHT"}},
    {"RPUSH", {"RPUSH", "key", "element [element ...]"}},
    {"RPUSHX", {"RPUSHX", "key", "element [element ...]"}},
    {"SADD", {"SADD", "key", "member [member ...]"}},
    {"SAVE", {"SAVE"}},
    {"SCARD", {"SCARD", "key"}},
    {"SCRIPT DEBUG", {"SCRIPT DEBUG", "YES|SYNC|NO"}},
    {"SCRIPT EXISTS", {"SCRIPT EXISTS", "sha1 [sha1 ...]"}},
    {"SCRIPT FLUSH", {"SCRIPT FLUSH", "[ASYNC|SYNC]"}},
    {"SCRIPT KILL", {"SCRIPT KILL"}},
    {"SCRIPT LOAD", {"SCRIPT LOAD", "script"}},
    {"SDIFF", {"SDIFF", "key [key ...]"}},
    {"SDIFFSTORE", {"SDIFFSTORE", "destination", "key [key ...]"}},
    {"SELECT", {"SELECT", "index"}},
    {"SET",
     {"SET", "key", "value",
      "[EX seconds|PX milliseconds|EXAT timestamp|PXAT milliseconds-timestamp|KEEPTTL]", "[NX|XX]",
      "[GET]"}},
    {"SETBIT", {"SETBIT", "key", "offset", "value"}},
    {"SETEX", {"SETEX", "key", "seconds", "value"}},
    {"SETNX", {"SETNX", "key", "value"}},
    {"SETRANGE", {"SETRANGE", "key", "offset", "value"}},
    {"SHUTDOWN", {"SHUTDOWN", "[NOSAVE|SAVE]"}},
    {"SINTER", {"SINTER", "key [key ...]"}},
    {"SINTERSTORE", {"SINTERSTORE", "destination", "key [key ...]"}},
    {"SISMEMBER", {"SISMEMBER", "key", "member"}},
    {"SMISMEMBER", {"SMISMEMBER", "key", "member [member ...]"}},
    {"SLAVEOF", {"SLAVEOF", "host", "port"}},
    {"REPLICAOF", {"REPLICAOF", "host", "port"}},
    {"SLOWLOG", {"SLOWLOG", "subcommand", "[argument]"}},
    {"SMEMBERS", {"SMEMBERS", "key"}},
    {"SMOVE", {"SMOVE", "source", "destination", "member"}},
    {"SORT",
     {"SORT", "key", "[BY pattern]", "[LIMIT offset count]", "[GET pattern [GET pattern ...]]",
      "[ASC|DESC]", "[ALPHA]", "[STORE destination]"}},
    {"SPOP", {"SPOP", "key", "[count]"}},
    {"SRANDMEMBER", {"SRANDMEMBER", "key", "[count]"}},
    {"SREM", {"SREM", "key", "member [member ...]"}},
    {"STRALGO", {"STRALGO", "LCS", "algo-specific-argument [algo-specific-argument ...]"}},
    {"STRLEN", {"STRLEN", "key"}},
    {"SUBSCRIBE", {"SUBSCRIBE", "channel [channel ...]"}},
    {"SUNION", {"SUNION", "key [key ...]"}},
    {"SUNIONSTORE", {"SUNIONSTORE", "destination", "key [key ...]"}},
    {"SWAPDB", {"SWAPDB", "index1", "index2"}},
    {"SYNC", {"SYNC"}},
    {"PSYNC", {"PSYNC", "replicationid", "offset"}},
    {"TIME", {"TIME"}},
    {"TOUCH", {"TOUCH", "key [key ...]"}},
    {"TTL", {"TTL", "key"}},
    {"TYPE", {"TYPE", "key"}},
    {"UNSUBSCRIBE", {"UNSUBSCRIBE", "[channel [channel ...]]"}},
    {"UNLINK", {"UNLINK", "key [key ...]"}},
    {"UNWATCH", {"UNWATCH"}},
    {"WAIT", {"WAIT", "numreplicas", "timeout"}},
    {"WATCH", {"WATCH", "key [key ...]"}},
    {"ZADD",
     {"ZADD", "key", "[NX|XX]", "[GT|LT]", "[CH]", "[INCR]", "score member [score member ...]"}},
    {"ZCARD", {"ZCARD", "key"}},
    {"ZCOUNT", {"ZCOUNT", "key", "min", "max"}},
    {"ZDIFF", {"ZDIFF", "numkeys", "key [key ...]", "[WITHSCORES]"}},
    {"ZDIFFSTORE", {"ZDIFFSTORE", "destination", "numkeys", "key [key ...]"}},
    {"ZINCRBY", {"ZINCRBY", "key", "increment", "member"}},
    {"ZINTER",
     {"ZINTER", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]", "[WITHSCORES]"}},
    {"ZINTERSTORE",
     {"ZINTERSTORE", "destination", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]"}},
    {"ZLEXCOUNT", {"ZLEXCOUNT", "key", "min", "max"}},
    {"ZPOPMAX", {"ZPOPMAX", "key", "[count]"}},
    {"ZPOPMIN", {"ZPOPMIN", "key", "[count]"}},
    {"ZRANDMEMBER", {"ZRANDMEMBER", "key", "[count [WITHSCORES]]"}},
    {"ZRANGESTORE",
     {"ZRANGESTORE", "dst", "src", "min", "max", "[BYSCORE|BYLEX]", "[REV]",
      "[LIMIT offset count]"}},
    {"ZRANGE",
     {"ZRANGE", "key", "min", "max", "[BYSCORE|BYLEX]", "[REV]", "[LIMIT offset count]",
      "[WITHSCORES]"}},
    {"ZRANGEBYLEX", {"ZRANGEBYLEX", "key", "min", "max", "[LIMIT offset count]"}},
    {"ZREVRANGEBYLEX", {"ZREVRANGEBYLEX", "key", "max", "min", "[LIMIT offset count]"}},
    {"ZRANGEBYSCORE",
     {"ZRANGEBYSCORE", "key", "min", "max", "[WITHSCORES]", "[LIMIT offset count]"}},
    {"ZRANK", {"ZRANK", "key", "member"}},
    {"ZREM", {"ZREM", "key", "member [member ...]"}},
    {"ZREMRANGEBYLEX", {"ZREMRANGEBYLEX", "key", "min", "max"}},
    {"ZREMRANGEBYRANK", {"ZREMRANGEBYRANK", "key", "start", "stop"}},
    {"ZREMRANGEBYSCORE", {"ZREMRANGEBYSCORE", "key", "min", "max"}},
    {"ZREVRANGE", {"ZREVRANGE", "key", "start", "stop", "[WITHSCORES]"}},
    {"ZREVRANGEBYSCORE",
     {"ZREVRANGEBYSCORE", "key", "max", "min", "[WITHSCORES]", "[LIMIT offset count]"}},
    {"ZREVRANK", {"ZREVRANK", "key", "member"}},
    {"ZSCORE", {"ZSCORE", "key", "member"}},
    {"ZUNION",
     {"ZUNION", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]", "[WITHSCORES]"}},
    {"ZMSCORE", {"ZMSCORE", "key", "member [member ...]"}},
    {"ZUNIONSTORE",
     {"ZUNIONSTORE", "destination", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]"}},
    {"SCAN", {"SCAN", "cursor", "[MATCH pattern]", "[COUNT count]", "[TYPE type]"}},
    {"SSCAN", {"SSCAN", "key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"HSCAN", {"HSCAN", "key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"ZSCAN", {"ZSCAN", "key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"XINFO", {"XINFO", "[CONSUMERS key groupname]", "[GROUPS key]", "[STREAM key]", "[HELP]"}},
    {"XADD",
     {"XADD", "key", "[NOMKSTREAM]", "[MAXLEN|MINID [=|~] threshold [LIMIT count]]", "*|ID",
      "field value [field value ...]"}},
    {"XTRIM", {"XTRIM", "key", "MAXLEN|MINID [=|~] threshold [LIMIT count]"}},
    {"XDEL", {"XDEL", "key", "ID [ID ...]"}},
    {"XRANGE", {"XRANGE", "key", "start", "end", "[COUNT count]"}},
    {"XREVRANGE", {"XREVRANGE", "key", "end", "start", "[COUNT count]"}},
    {"XLEN", {"XLEN", "key"}},
    {"XREAD",
     {"XREAD", "[COUNT count]", "[BLOCK milliseconds]", "STREAMS", "key [key ...]", "ID [ID ...]"}},
    {"XGROUP",
     {"XGROUP", "[CREATE key groupname ID|$ [MKSTREAM]]", "[SETID key groupname ID|$]",
      "[DESTROY key groupname]", "[CREATECONSUMER key groupname consumername]",
      "[DELCONSUMER key groupname consumername]"}},
    {"XREADGROUP",
     {"XREADGROUP", "GROUP group consumer", "[COUNT count]", "[BLOCK milliseconds]", "[NOACK]",
      "STREAMS", "key [key ...]", "ID [ID ...]"}},
    {"XACK", {"XACK", "key", "group", "ID [ID ...]"}},
    {"XCLAIM",
     {"XCLAIM", "key", "group", "consumer", "min-idle-time", "ID [ID ...]", "[IDLE ms]",
      "[TIME ms-unix-time]", "[RETRYCOUNT count]", "[FORCE]", "[JUSTID]"}},
    {"XAUTOCLAIM",
     {"XAUTOCLAIM", "key", "group", "consumer", "min-idle-time", "start", "[COUNT count]",
      "[JUSTID]"}},
    {"XPENDING", {"XPENDING", "key", "group", "[[IDLE min-idle-time] start end count [consumer]]"}},
    {"LATENCY DOCTOR", {"LATENCY DOCTOR"}},
    {"LATENCY GRAPH", {"LATENCY GRAPH", "event"}},
    {"LATENCY HISTORY", {"LATENCY HISTORY", "event"}},
    {"LATENCY LATEST", {"LATENCY LATEST"}},
    {"LATENCY RESET", {"LATENCY RESET", "[event [event ...]]"}},
    {"LATENCY HELP", {"LATENCY HELP"}},

    // The following commands are manually added.

    // Additional commands used in Redis sentinel mode.
    {"SENTINEL", {"SENTINEL"}},
    // Synchronous replication: http://antirez.com/news/58
    {"REPLCONF ACK", {"REPLCONF ACK"}},
};

StatusOr<int> ParseSize(BinaryDecoder* decoder) {
  PL_ASSIGN_OR_RETURN(std::string_view size_str, decoder->ExtractStringUntil(kTerminalSequence));

  constexpr size_t kSizeStrMaxLen = 16;
  if (size_str.size() > kSizeStrMaxLen) {
    return error::InvalidArgument(
        "Redis size string is longer than $0, which indicates traffic is misclassified as Redis.",
        kSizeStrMaxLen);
  }

  // Length could be -1, which stands for NULL, and means the value is not set.
  // That's different than an empty string, which length is 0.
  // So here we initialize the value to -2.
  int size = -2;
  if (!absl::SimpleAtoi(size_str, &size)) {
    return error::InvalidArgument("String '$0' cannot be parsed as integer", size_str);
  }

  if (size < kNullSize) {
    return error::InvalidArgument("Size cannot be less than $0, got '$1'", kNullSize, size_str);
  }

  return size;
}

// Bulk string is formatted as <length>\r\n<actual string, up to 512MB>\r\n
Status ParseBulkString(BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(int len, ParseSize(decoder));

  constexpr int kMaxLen = 512 * 1024 * 1024;
  if (len > kMaxLen) {
    return error::InvalidArgument("Length cannot be larger than 512MB, got '$0'", len);
  }

  if (len == kNullSize) {
    constexpr std::string_view kNullBulkString = "<NULL>";
    // TODO(yzhao): This appears wrong, as Redis has NULL value, here "<NULL>" is presented as
    // a string. ATM don't know how to output NULL value in Rapidjson. Research and update this.
    msg->payload = kNullBulkString;
    return Status::OK();
  }

  PL_ASSIGN_OR_RETURN(std::string_view payload,
                      decoder->ExtractString(len + kTerminalSequence.size()));
  if (!absl::EndsWith(payload, kTerminalSequence)) {
    return error::InvalidArgument("Bulk string should be terminated by '$0'", kTerminalSequence);
  }
  payload.remove_suffix(kTerminalSequence.size());
  msg->payload = payload;
  return Status::OK();
}

// Holds the command name and the description of its arguments.
struct CmdAndArgs {
  const std::string_view name;
  const CmdArgs* args;
};

using CmdAndArgsPair = absl::flat_hash_map<std::string_view, CmdArgs>::value_type;

std::optional<CmdAndArgs> GetCmdAndArgs(std::string_view payload) {
  auto iter = kCmdList.find(payload);
  if (iter != kCmdList.end()) {
    return CmdAndArgs{iter->first, &iter->second};
  }
  return std::nullopt;
}

std::optional<CmdAndArgs> GetCmdAndArgs(VectorView<std::string>* payloads) {
  if (payloads->empty()) {
    return std::nullopt;
  }
  // Search the double-words command first.
  if (payloads->size() >= 2) {
    std::string candidate_cmd =
        absl::AsciiStrToUpper(absl::StrCat((*payloads)[0], " ", (*payloads)[1]));
    auto res_opt = GetCmdAndArgs(candidate_cmd);
    if (res_opt.has_value()) {
      payloads->pop_front(2);
      return res_opt;
    }
  }
  std::string candidate_cmd = absl::AsciiStrToUpper(payloads->front());
  auto res_opt = GetCmdAndArgs(candidate_cmd);
  if (res_opt.has_value()) {
    payloads->pop_front(1);
  }
  return res_opt;
}

bool IsPubMsg(const std::vector<std::string>& payloads) {
  // Published message format is at https://redis.io/topics/pubsub#format-of-pushed-messages
  constexpr size_t kArrayPayloadSize = 3;
  if (payloads.size() < kArrayPayloadSize) {
    return false;
  }
  constexpr std::string_view kMessageStr = "MESSAGE";
  if (absl::AsciiStrToUpper(payloads.front()) != kMessageStr) {
    return false;
  }
  return true;
}

// This calls ParseMessage(), which eventually calls ParseArray() and are both recursive
// functions. This is because Array message can include nested array messages.
Status ParseArray(MessageType type, BinaryDecoder* decoder, Message* msg);

Status ParseMessage(MessageType type, BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(const char type_marker, decoder->ExtractChar());

  switch (type_marker) {
    case kSimpleStringMarker: {
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = str;
      break;
    }
    case kBulkStringsMarker: {
      PL_RETURN_IF_ERROR(ParseBulkString(decoder, msg));
      break;
    }
    case kErrorMarker: {
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = str;
      break;
    }
    case kIntegerMarker: {
      PL_ASSIGN_OR_RETURN(msg->payload, decoder->ExtractStringUntil(kTerminalSequence));
      break;
    }
    case kArrayMarker: {
      PL_RETURN_IF_ERROR(ParseArray(type, decoder, msg));
      break;
    }
    default:
      return error::InvalidArgument("Unexpected Redis type marker char (displayed as integer): %d",
                                    type_marker);
  }
  // This is needed for GCC build.
  return Status::OK();
}

// Array is formatted as *<size_str>\r\n[one of simple string, error, bulk string, etc.]
Status ParseArray(MessageType type, BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(int len, ParseSize(decoder));

  if (len == kNullSize) {
    constexpr std::string_view kNullArray = "[NULL]";
    msg->payload = kNullArray;
    return Status::OK();
  }

  std::vector<std::string> payloads;
  for (int i = 0; i < len; ++i) {
    Message tmp;
    PL_RETURN_IF_ERROR(ParseMessage(type, decoder, &tmp));
    payloads.push_back(std::move(tmp.payload));
  }

  auto payloads_view = VectorView<std::string>(payloads);

  // Redis wire protocol said requests are array consisting of bulk strings:
  // https://redis.io/topics/protocol#sending-commands-to-a-redis-server
  if (type == MessageType::kRequest) {
    std::optional<CmdAndArgs> cmd_and_args_opt = GetCmdAndArgs(&payloads_view);
    if (cmd_and_args_opt.has_value()) {
      msg->command = cmd_and_args_opt.value().name;
      msg->payload = cmd_and_args_opt.value().args->FmtArgs(payloads_view);
    } else {
      msg->payload = FormatAsJSONArray(payloads_view);
    }
  } else {
    msg->payload = FormatAsJSONArray(payloads_view);
  }

  if (type == MessageType::kResponse && IsPubMsg(payloads)) {
    msg->is_published_message = true;
  }

  return Status::OK();
}

ParseState TranslateErrorStatus(const Status& status) {
  if (error::IsNotFound(status) || error::IsResourceUnavailable(status)) {
    return ParseState::kNeedsMoreData;
  }
  if (!status.ok()) {
    return ParseState::kInvalid;
  }
  DCHECK(false) << "Can only translate NotFound or ResourceUnavailable error, got: "
                << status.ToString();
  return ParseState::kSuccess;
}

}  // namespace

size_t FindMessageBoundary(std::string_view buf, size_t start_pos) {
  for (; start_pos < buf.size(); ++start_pos) {
    const char type_marker = buf[start_pos];
    if (type_marker == kSimpleStringMarker || type_marker == kErrorMarker ||
        type_marker == kIntegerMarker || type_marker == kBulkStringsMarker ||
        type_marker == kArrayMarker) {
      return start_pos;
    }
  }
  return std::string_view::npos;
}

// Redis protocol specification: https://redis.io/topics/protocol
// This can also be implemented as a recursive function.
ParseState ParseMessage(MessageType type, std::string_view* buf, Message* msg) {
  BinaryDecoder decoder(*buf);

  auto status = ParseMessage(type, &decoder, msg);

  if (!status.ok()) {
    return TranslateErrorStatus(status);
  }

  *buf = decoder.Buf();

  return ParseState::kSuccess;
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
