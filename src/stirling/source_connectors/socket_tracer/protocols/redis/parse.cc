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

// Formats the input arguments as JSON array.
static std::string FormatAsJSONArray(VectorView<std::string> args) {
  std::vector<std::string_view> args_copy = {args.begin(), args.end()};
  return ToJSONString(args_copy);
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
    cmd_args_.insert(cmd_args_.end(), cmd_args.begin(), cmd_args.end());
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
    for (char c : arg_name) {
      if (!islower(c)) {
        return false;
      }
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

  std::vector<std::string_view> cmd_args_;
  std::optional<std::vector<ArgDesc>> cmd_arg_descs_;
};

// This list is produced with by:
//   //src/stirling/source_connectors/socket_tracer/protocols/redis:redis_cmds_format_generator
//
//   Read its help message to get the instructions.
const absl::flat_hash_map<std::string_view, CmdArgs> kCmdList = {
    {"ACL LOAD", {}},
    {"ACL SAVE", {}},
    {"ACL LIST", {}},
    {"ACL USERS", {}},
    {"ACL GETUSER", {"username"}},
    {"ACL SETUSER", {"username", "[rule [rule ...]]"}},
    {"ACL DELUSER", {"username [username ...]"}},
    {"ACL CAT", {"[categoryname]"}},
    {"ACL GENPASS", {"[bits]"}},
    {"ACL WHOAMI", {}},
    {"ACL LOG", {"[count or RESET]"}},
    {"ACL HELP", {}},
    {"APPEND", {"key", "value"}},
    {"AUTH", {"[username]", "password"}},
    {"BGREWRITEAOF", {}},
    {"BGSAVE", {"[SCHEDULE]"}},
    {"BITCOUNT", {"key", "[start end]"}},
    {"BITFIELD",
     {"key", "[GET type offset]", "[SET type offset value]", "[INCRBY type offset increment]",
      "[OVERFLOW WRAP|SAT|FAIL]"}},
    {"BITOP", {"operation", "destkey", "key [key ...]"}},
    {"BITPOS", {"key", "bit", "[start]", "[end]"}},
    {"BLPOP", {"key [key ...]", "timeout"}},
    {"BRPOP", {"key [key ...]", "timeout"}},
    {"BRPOPLPUSH", {"source", "destination", "timeout"}},
    {"BLMOVE", {"source", "destination", "LEFT|RIGHT", "LEFT|RIGHT", "timeout"}},
    {"BZPOPMIN", {"key [key ...]", "timeout"}},
    {"BZPOPMAX", {"key [key ...]", "timeout"}},
    {"CLIENT CACHING", {"YES|NO"}},
    {"CLIENT ID", {}},
    {"CLIENT INFO", {}},
    {"CLIENT KILL",
     {"[ip:port]", "[ID client-id]", "[TYPE normal|master|slave|pubsub]", "[USER username]",
      "[ADDR ip:port]", "[SKIPME yes/no]"}},
    {"CLIENT LIST", {"[TYPE normal|master|replica|pubsub]", "[ID client-id [client-id ...]]"}},
    {"CLIENT GETNAME", {}},
    {"CLIENT GETREDIR", {}},
    {"CLIENT UNPAUSE", {}},
    {"CLIENT PAUSE", {"timeout", "[WRITE|ALL]"}},
    {"CLIENT REPLY", {"ON|OFF|SKIP"}},
    {"CLIENT SETNAME", {"connection-name"}},
    {"CLIENT TRACKING",
     {"ON|OFF", "[REDIRECT client-id]", "[PREFIX prefix [PREFIX prefix ...]]", "[BCAST]", "[OPTIN]",
      "[OPTOUT]", "[NOLOOP]"}},
    {"CLIENT TRACKINGINFO", {}},
    {"CLIENT UNBLOCK", {"client-id", "[TIMEOUT|ERROR]"}},
    {"CLUSTER ADDSLOTS", {"slot [slot ...]"}},
    {"CLUSTER BUMPEPOCH", {}},
    {"CLUSTER COUNT-FAILURE-REPORTS", {"node-id"}},
    {"CLUSTER COUNTKEYSINSLOT", {"slot"}},
    {"CLUSTER DELSLOTS", {"slot [slot ...]"}},
    {"CLUSTER FAILOVER", {"[FORCE|TAKEOVER]"}},
    {"CLUSTER FLUSHSLOTS", {}},
    {"CLUSTER FORGET", {"node-id"}},
    {"CLUSTER GETKEYSINSLOT", {"slot", "count"}},
    {"CLUSTER INFO", {}},
    {"CLUSTER KEYSLOT", {"key"}},
    {"CLUSTER MEET", {"ip", "port"}},
    {"CLUSTER MYID", {}},
    {"CLUSTER NODES", {}},
    {"CLUSTER REPLICATE", {"node-id"}},
    {"CLUSTER RESET", {"[HARD|SOFT]"}},
    {"CLUSTER SAVECONFIG", {}},
    {"CLUSTER SET-CONFIG-EPOCH", {"config-epoch"}},
    {"CLUSTER SETSLOT", {"slot", "IMPORTING|MIGRATING|STABLE|NODE", "[node-id]"}},
    {"CLUSTER SLAVES", {"node-id"}},
    {"CLUSTER REPLICAS", {"node-id"}},
    {"CLUSTER SLOTS", {}},
    {"COMMAND", {}},
    {"COMMAND COUNT", {}},
    {"COMMAND GETKEYS", {}},
    {"COMMAND INFO", {"command-name [command-name ...]"}},
    {"CONFIG GET", {"parameter"}},
    {"CONFIG REWRITE", {}},
    {"CONFIG SET", {"parameter", "value"}},
    {"CONFIG RESETSTAT", {}},
    {"COPY", {"source", "destination", "[DB destination-db]", "[REPLACE]"}},
    {"DBSIZE", {}},
    {"DEBUG OBJECT", {"key"}},
    {"DEBUG SEGFAULT", {}},
    {"DECR", {"key"}},
    {"DECRBY", {"key", "decrement"}},
    {"DEL", {"key [key ...]"}},
    {"DISCARD", {}},
    {"DUMP", {"key"}},
    {"ECHO", {"message"}},
    {"EVAL", {"script", "numkeys", "key [key ...]", "arg [arg ...]"}},
    {"EVALSHA", {"sha1", "numkeys", "key [key ...]", "arg [arg ...]"}},
    {"EXEC", {}},
    {"EXISTS", {"key [key ...]"}},
    {"EXPIRE", {"key", "seconds"}},
    {"EXPIREAT", {"key", "timestamp"}},
    {"FLUSHALL", {"[ASYNC]"}},
    {"FLUSHDB", {"[ASYNC]"}},
    {"GEOADD",
     {"key", "[NX|XX]", "[CH]", "longitude latitude member [longitude latitude member ...]"}},
    {"GEOHASH", {"key", "member [member ...]"}},
    {"GEOPOS", {"key", "member [member ...]"}},
    {"GEODIST", {"key", "member1", "member2", "[m|km|ft|mi]"}},
    {"GEORADIUS",
     {"key", "longitude", "latitude", "radius", "m|km|ft|mi", "[WITHCOORD]", "[WITHDIST]",
      "[WITHHASH]", "[COUNT count [ANY]]", "[ASC|DESC]", "[STORE key]", "[STOREDIST key]"}},
    {"GEORADIUSBYMEMBER",
     {"key", "member", "radius", "m|km|ft|mi", "[WITHCOORD]", "[WITHDIST]", "[WITHHASH]",
      "[COUNT count [ANY]]", "[ASC|DESC]", "[STORE key]", "[STOREDIST key]"}},
    {"GEOSEARCH",
     {"key", "[FROMMEMBER member]", "[FROMLONLAT longitude latitude]",
      "[BYRADIUS radius m|km|ft|mi]", "[BYBOX width height m|km|ft|mi]", "[ASC|DESC]",
      "[COUNT count [ANY]]", "[WITHCOORD]", "[WITHDIST]", "[WITHHASH]"}},
    {"GEOSEARCHSTORE",
     {"destination", "source", "[FROMMEMBER member]", "[FROMLONLAT longitude latitude]",
      "[BYRADIUS radius m|km|ft|mi]", "[BYBOX width height m|km|ft|mi]", "[ASC|DESC]",
      "[COUNT count [ANY]]", "[WITHCOORD]", "[WITHDIST]", "[WITHHASH]", "[STOREDIST]"}},
    {"GET", {"key"}},
    {"GETBIT", {"key", "offset"}},
    {"GETRANGE", {"key", "start", "end"}},
    {"GETSET", {"key", "value"}},
    {"HDEL", {"key", "field [field ...]"}},
    {"HELLO", {"[protover [AUTH username password] [SETNAME clientname]]"}},
    {"HEXISTS", {"key", "field"}},
    {"HGET", {"key", "field"}},
    {"HGETALL", {"key"}},
    {"HINCRBY", {"key", "field", "increment"}},
    {"HINCRBYFLOAT", {"key", "field", "increment"}},
    {"HKEYS", {"key"}},
    {"HLEN", {"key"}},
    {"HMGET", {"key", "field [field ...]"}},
    {"HMSET", {"key", "field value [field value ...]"}},
    {"HSET", {"key", "field value [field value ...]"}},
    {"HSETNX", {"key", "field", "value"}},
    {"HSTRLEN", {"key", "field"}},
    {"HVALS", {"key"}},
    {"INCR", {"key"}},
    {"INCRBY", {"key", "increment"}},
    {"INCRBYFLOAT", {"key", "increment"}},
    {"INFO", {"[section]"}},
    {"LOLWUT", {"[VERSION version]"}},
    {"KEYS", {"pattern"}},
    {"LASTSAVE", {}},
    {"LINDEX", {"key", "index"}},
    {"LINSERT", {"key", "BEFORE|AFTER", "pivot", "element"}},
    {"LLEN", {"key"}},
    {"LPOP", {"key", "[count]"}},
    {"LPOS", {"key", "element", "[RANK rank]", "[COUNT num-matches]", "[MAXLEN len]"}},
    {"LPUSH", {"key", "element [element ...]"}},
    {"LPUSHX", {"key", "element [element ...]"}},
    {"LRANGE", {"key", "start", "stop"}},
    {"LREM", {"key", "count", "element"}},
    {"LSET", {"key", "index", "element"}},
    {"LTRIM", {"key", "start", "stop"}},
    {"MEMORY DOCTOR", {}},
    {"MEMORY HELP", {}},
    {"MEMORY MALLOC-STATS", {}},
    {"MEMORY PURGE", {}},
    {"MEMORY STATS", {}},
    {"MEMORY USAGE", {"key", "[SAMPLES count]"}},
    {"MGET", {"key [key ...]"}},
    {"MIGRATE",
     {"host", "port", R"(key|"")", "destination-db", "timeout", "[COPY]", "[REPLACE]",
      "[AUTH password]", "[AUTH2 username password]", "[KEYS key [key ...]]"}},
    {"MODULE LIST", {}},
    {"MODULE LOAD", {"path", "[ arg [arg ...]]"}},
    {"MODULE UNLOAD", {"name"}},
    {"MONITOR", {}},
    {"MOVE", {"key", "db"}},
    {"MSET", {"key value [key value ...]"}},
    {"MSETNX", {"key value [key value ...]"}},
    {"MULTI", {}},
    {"OBJECT", {"subcommand", "[arguments [arguments ...]]"}},
    {"PERSIST", {"key"}},
    {"PEXPIRE", {"key", "milliseconds"}},
    {"PEXPIREAT", {"key", "milliseconds-timestamp"}},
    {"PFADD", {"key", "element [element ...]"}},
    {"PFCOUNT", {"key [key ...]"}},
    {"PFMERGE", {"destkey", "sourcekey [sourcekey ...]"}},
    {"PING", {"[message]"}},
    {"PSETEX", {"key", "milliseconds", "value"}},
    {"PSUBSCRIBE", {"pattern [pattern ...]"}},
    {"PUBSUB", {"subcommand", "[argument [argument ...]]"}},
    {"PTTL", {"key"}},
    {"PUBLISH", {"channel", "message"}},
    {"PUNSUBSCRIBE", {"[pattern [pattern ...]]"}},
    {"QUIT", {}},
    {"RANDOMKEY", {}},
    {"READONLY", {}},
    {"READWRITE", {}},
    {"RENAME", {"key", "newkey"}},
    {"RENAMENX", {"key", "newkey"}},
    {"RESET", {}},
    {"RESTORE",
     {"key", "ttl", "serialized-value", "[REPLACE]", "[ABSTTL]", "[IDLETIME seconds]",
      "[FREQ frequency]"}},
    {"ROLE", {}},
    {"RPOP", {"key", "[count]"}},
    {"RPOPLPUSH", {"source", "destination"}},
    {"LMOVE", {"source", "destination", "LEFT|RIGHT", "LEFT|RIGHT"}},
    {"RPUSH", {"key", "element [element ...]"}},
    {"RPUSHX", {"key", "element [element ...]"}},
    {"SADD", {"key", "member [member ...]"}},
    {"SAVE", {}},
    {"SCARD", {"key"}},
    {"SCRIPT DEBUG", {"YES|SYNC|NO"}},
    {"SCRIPT EXISTS", {"sha1 [sha1 ...]"}},
    {"SCRIPT FLUSH", {}},
    {"SCRIPT KILL", {}},
    {"SCRIPT LOAD", {"script"}},
    {"SDIFF", {"key [key ...]"}},
    {"SDIFFSTORE", {"destination", "key [key ...]"}},
    {"SELECT", {"index"}},
    {"SET", {"key", "value", "[EX seconds|PX milliseconds|KEEPTTL]", "[NX|XX]", "[GET]"}},
    {"SETBIT", {"key", "offset", "value"}},
    {"SETEX", {"key", "seconds", "value"}},
    {"SETNX", {"key", "value"}},
    {"SETRANGE", {"key", "offset", "value"}},
    {"SHUTDOWN", {"[NOSAVE|SAVE]"}},
    {"SINTER", {"key [key ...]"}},
    {"SINTERSTORE", {"destination", "key [key ...]"}},
    {"SISMEMBER", {"key", "member"}},
    {"SMISMEMBER", {"key", "member [member ...]"}},
    {"SLAVEOF", {"host", "port"}},
    {"REPLICAOF", {"host", "port"}},
    {"SLOWLOG", {"subcommand", "[argument]"}},
    {"SMEMBERS", {"key"}},
    {"SMOVE", {"source", "destination", "member"}},
    {"SORT",
     {"key", "[BY pattern]", "[LIMIT offset count]", "[GET pattern [GET pattern ...]]",
      "[ASC|DESC]", "[ALPHA]", "[STORE destination]"}},
    {"SPOP", {"key", "[count]"}},
    {"SRANDMEMBER", {"key", "[count]"}},
    {"SREM", {"key", "member [member ...]"}},
    {"STRALGO", {"LCS", "algo-specific-argument [algo-specific-argument ...]"}},
    {"STRLEN", {"key"}},
    {"SUBSCRIBE", {"channel [channel ...]"}},
    {"SUNION", {"key [key ...]"}},
    {"SUNIONSTORE", {"destination", "key [key ...]"}},
    {"SWAPDB", {"index1", "index2"}},
    {"SYNC", {}},
    {"PSYNC", {"replicationid", "offset"}},
    {"TIME", {}},
    {"TOUCH", {"key [key ...]"}},
    {"TTL", {"key"}},
    {"TYPE", {"key"}},
    {"UNSUBSCRIBE", {"[channel [channel ...]]"}},
    {"UNLINK", {"key [key ...]"}},
    {"UNWATCH", {}},
    {"WAIT", {"numreplicas", "timeout"}},
    {"WATCH", {"key [key ...]"}},
    {"ZADD", {"key", "[NX|XX]", "[GT|LT]", "[CH]", "[INCR]", "score member [score member ...]"}},
    {"ZCARD", {"key"}},
    {"ZCOUNT", {"key", "min", "max"}},
    {"ZDIFF", {"numkeys", "key [key ...]", "[WITHSCORES]"}},
    {"ZDIFFSTORE", {"destination", "numkeys", "key [key ...]"}},
    {"ZINCRBY", {"key", "increment", "member"}},
    {"ZINTER",
     {"numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]", "[AGGREGATE SUM|MIN|MAX]",
      "[WITHSCORES]"}},
    {"ZINTERSTORE",
     {"destination", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]"}},
    {"ZLEXCOUNT", {"key", "min", "max"}},
    {"ZPOPMAX", {"key", "[count]"}},
    {"ZPOPMIN", {"key", "[count]"}},
    {"ZRANGESTORE",
     {"dst", "src", "min", "max", "[BYSCORE|BYLEX]", "[REV]", "[LIMIT offset count]"}},
    {"ZRANGE",
     {"key", "min", "max", "[BYSCORE|BYLEX]", "[REV]", "[LIMIT offset count]", "[WITHSCORES]"}},
    {"ZRANGEBYLEX", {"key", "min", "max", "[LIMIT offset count]"}},
    {"ZREVRANGEBYLEX", {"key", "max", "min", "[LIMIT offset count]"}},
    {"ZRANGEBYSCORE", {"key", "min", "max", "[WITHSCORES]", "[LIMIT offset count]"}},
    {"ZRANK", {"key", "member"}},
    {"ZREM", {"key", "member [member ...]"}},
    {"ZREMRANGEBYLEX", {"key", "min", "max"}},
    {"ZREMRANGEBYRANK", {"key", "start", "stop"}},
    {"ZREMRANGEBYSCORE", {"key", "min", "max"}},
    {"ZREVRANGE", {"key", "start", "stop", "[WITHSCORES]"}},
    {"ZREVRANGEBYSCORE", {"key", "max", "min", "[WITHSCORES]", "[LIMIT offset count]"}},
    {"ZREVRANK", {"key", "member"}},
    {"ZSCORE", {"key", "member"}},
    {"ZUNION",
     {"numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]", "[AGGREGATE SUM|MIN|MAX]",
      "[WITHSCORES]"}},
    {"ZMSCORE", {"key", "member [member ...]"}},
    {"ZUNIONSTORE",
     {"destination", "numkeys", "key [key ...]", "[WEIGHTS weight [weight ...]]",
      "[AGGREGATE SUM|MIN|MAX]"}},
    {"SCAN", {"cursor", "[MATCH pattern]", "[COUNT count]", "[TYPE type]"}},
    {"SSCAN", {"key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"HSCAN", {"key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"ZSCAN", {"key", "cursor", "[MATCH pattern]", "[COUNT count]"}},
    {"XINFO", {"[CONSUMERS key groupname]", "[GROUPS key]", "[STREAM key]", "[HELP]"}},
    {"XADD",
     {"key", "[NOMKSTREAM]", "[MAXLEN|MINID [=|~] threshold [LIMIT count]]", "*|ID",
      "field value [field value ...]"}},
    {"XTRIM", {"key", "MAXLEN|MINID [=|~] threshold [LIMIT count]"}},
    {"XDEL", {"key", "ID [ID ...]"}},
    {"XRANGE", {"key", "start", "end", "[COUNT count]"}},
    {"XREVRANGE", {"key", "end", "start", "[COUNT count]"}},
    {"XLEN", {"key"}},
    {"XREAD", {"[COUNT count]", "[BLOCK milliseconds]", "STREAMS", "key [key ...]", "ID [ID ...]"}},
    {"XGROUP",
     {"[CREATE key groupname ID|$ [MKSTREAM]]", "[SETID key groupname ID|$]",
      "[DESTROY key groupname]", "[CREATECONSUMER key groupname consumername]",
      "[DELCONSUMER key groupname consumername]"}},
    {"XREADGROUP",
     {"GROUP group consumer", "[COUNT count]", "[BLOCK milliseconds]", "[NOACK]", "STREAMS",
      "key [key ...]", "ID [ID ...]"}},
    {"XACK", {"key", "group", "ID [ID ...]"}},
    {"XCLAIM",
     {"key", "group", "consumer", "min-idle-time", "ID [ID ...]", "[IDLE ms]",
      "[TIME ms-unix-time]", "[RETRYCOUNT count]", "[FORCE]", "[JUSTID]"}},
    {"XAUTOCLAIM",
     {"key", "group", "consumer", "min-idle-time", "start", "[COUNT count]", "[JUSTID]"}},
    {"XPENDING", {"key", "group", "[[IDLE min-idle-time] start end count [consumer]]"}},
    {"LATENCY DOCTOR", {}},
    {"LATENCY GRAPH", {"event"}},
    {"LATENCY HISTORY", {"event"}},
    {"LATENCY LATEST", {}},
    {"LATENCY RESET", {"[event [event ...]]"}},
    {"LATENCY HELP", {}},

    // The following commands are manually added.

    // Additional commands used in Redis sentinel mode.
    {"SENTINEL", {}},
    // Synchronous replication: http://antirez.com/news/58
    {"REPLCONF ACK", {}},
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
