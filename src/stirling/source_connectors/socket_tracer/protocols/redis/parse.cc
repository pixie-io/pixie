#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

constexpr char kSimpleStringMarker = '+';
constexpr char kErrorMarker = '-';
constexpr char kIntegerMarker = ':';
constexpr char kBulkStringsMarker = '$';
constexpr char kArrayMarker = '*';
// This is Redis' universal terminating sequence.
constexpr std::string_view kTerminalSequence = "\r\n";
constexpr int kNullSize = -1;

// This list is produced with (turned into upper case for :
//   curl https://redis.io/commands >redis_commands
//   xmllint --html --xpath '//span[@class="command"]/text()' redis_commands | grep -o "\S.*\S" |
//   awk '{print "\""$0"\","}'
const absl::flat_hash_set<std::string_view> kCmdList = {
    "ACL LOAD",
    "ACL SAVE",
    "ACL LIST",
    "ACL USERS",
    "ACL GETUSER",
    "ACL SETUSER",
    "ACL DELUSER",
    "ACL CAT",
    "ACL GENPASS",
    "ACL WHOAMI",
    "ACL LOG",
    "ACL HELP",
    "APPEND",
    "AUTH",
    "BGREWRITEAOF",
    "BGSAVE",
    "BITCOUNT",
    "BITFIELD",
    "BITOP",
    "BITPOS",
    "BLPOP",
    "BRPOP",
    "BRPOPLPUSH",
    "BLMOVE",
    "BZPOPMIN",
    "BZPOPMAX",
    "CLIENT CACHING",
    "CLIENT ID",
    "CLIENT INFO",
    "CLIENT KILL",
    "CLIENT LIST",
    "CLIENT GETNAME",
    "CLIENT GETREDIR",
    "CLIENT UNPAUSE",
    "CLIENT PAUSE",
    "CLIENT REPLY",
    "CLIENT SETNAME",
    "CLIENT TRACKING",
    "CLIENT TRACKINGINFO",
    "CLIENT UNBLOCK",
    "CLUSTER ADDSLOTS",
    "CLUSTER BUMPEPOCH",
    "CLUSTER COUNT-FAILURE-REPORTS",
    "CLUSTER COUNTKEYSINSLOT",
    "CLUSTER DELSLOTS",
    "CLUSTER FAILOVER",
    "CLUSTER FLUSHSLOTS",
    "CLUSTER FORGET",
    "CLUSTER GETKEYSINSLOT",
    "CLUSTER INFO",
    "CLUSTER KEYSLOT",
    "CLUSTER MEET",
    "CLUSTER MYID",
    "CLUSTER NODES",
    "CLUSTER REPLICATE",
    "CLUSTER RESET",
    "CLUSTER SAVECONFIG",
    "CLUSTER SET-CONFIG-EPOCH",
    "CLUSTER SETSLOT",
    "CLUSTER SLAVES",
    "CLUSTER REPLICAS",
    "CLUSTER SLOTS",
    "COMMAND",
    "COMMAND COUNT",
    "COMMAND GETKEYS",
    "COMMAND INFO",
    "CONFIG GET",
    "CONFIG REWRITE",
    "CONFIG SET",
    "CONFIG RESETSTAT",
    "COPY",
    "DBSIZE",
    "DEBUG OBJECT",
    "DEBUG SEGFAULT",
    "DECR",
    "DECRBY",
    "DEL",
    "DISCARD",
    "DUMP",
    "ECHO",
    "EVAL",
    "EVALSHA",
    "EXEC",
    "EXISTS",
    "EXPIRE",
    "EXPIREAT",
    "FLUSHALL",
    "FLUSHDB",
    "GEOADD",
    "GEOHASH",
    "GEOPOS",
    "GEODIST",
    "GEORADIUS",
    "GEORADIUSBYMEMBER",
    "GEOSEARCH",
    "GEOSEARCHSTORE",
    "GET",
    "GETBIT",
    "GETRANGE",
    "GETSET",
    "HDEL",
    "HELLO",
    "HEXISTS",
    "HGET",
    "HGETALL",
    "HINCRBY",
    "HINCRBYFLOAT",
    "HKEYS",
    "HLEN",
    "HMGET",
    "HMSET",
    "HSET",
    "HSETNX",
    "HSTRLEN",
    "HVALS",
    "INCR",
    "INCRBY",
    "INCRBYFLOAT",
    "INFO",
    "LOLWUT",
    "KEYS",
    "LASTSAVE",
    "LINDEX",
    "LINSERT",
    "LLEN",
    "LPOP",
    "LPOS",
    "LPUSH",
    "LPUSHX",
    "LRANGE",
    "LREM",
    "LSET",
    "LTRIM",
    "MEMORY DOCTOR",
    "MEMORY HELP",
    "MEMORY MALLOC-STATS",
    "MEMORY PURGE",
    "MEMORY STATS",
    "MEMORY USAGE",
    "MGET",
    "MIGRATE",
    "MODULE LIST",
    "MODULE LOAD",
    "MODULE UNLOAD",
    "MONITOR",
    "MOVE",
    "MSET",
    "MSETNX",
    "MULTI",
    "OBJECT",
    "PERSIST",
    "PEXPIRE",
    "PEXPIREAT",
    "PFADD",
    "PFCOUNT",
    "PFMERGE",
    "PING",
    "PSETEX",
    "PSUBSCRIBE",
    "PUBSUB",
    "PTTL",
    "PUBLISH",
    "PUNSUBSCRIBE",
    "QUIT",
    "RANDOMKEY",
    "READONLY",
    "READWRITE",
    "RENAME",
    "RENAMENX",
    "RESET",
    "RESTORE",
    "ROLE",
    "RPOP",
    "RPOPLPUSH",
    "LMOVE",
    "RPUSH",
    "RPUSHX",
    "SADD",
    "SAVE",
    "SCARD",
    "SCRIPT DEBUG",
    "SCRIPT EXISTS",
    "SCRIPT FLUSH",
    "SCRIPT KILL",
    "SCRIPT LOAD",
    "SDIFF",
    "SDIFFSTORE",
    "SELECT",
    "SET",
    "SETBIT",
    "SETEX",
    "SETNX",
    "SETRANGE",
    "SHUTDOWN",
    "SINTER",
    "SINTERSTORE",
    "SISMEMBER",
    "SMISMEMBER",
    "SLAVEOF",
    "REPLICAOF",
    "SLOWLOG",
    "SMEMBERS",
    "SMOVE",
    "SORT",
    "SPOP",
    "SRANDMEMBER",
    "SREM",
    "STRALGO",
    "STRLEN",
    "SUBSCRIBE",
    "SUNION",
    "SUNIONSTORE",
    "SWAPDB",
    "SYNC",
    "PSYNC",
    "TIME",
    "TOUCH",
    "TTL",
    "TYPE",
    "UNSUBSCRIBE",
    "UNLINK",
    "UNWATCH",
    "WAIT",
    "WATCH",
    "ZADD",
    "ZCARD",
    "ZCOUNT",
    "ZDIFF",
    "ZDIFFSTORE",
    "ZINCRBY",
    "ZINTER",
    "ZINTERSTORE",
    "ZLEXCOUNT",
    "ZPOPMAX",
    "ZPOPMIN",
    "ZRANGESTORE",
    "ZRANGE",
    "ZRANGEBYLEX",
    "ZREVRANGEBYLEX",
    "ZRANGEBYSCORE",
    "ZRANK",
    "ZREM",
    "ZREMRANGEBYLEX",
    "ZREMRANGEBYRANK",
    "ZREMRANGEBYSCORE",
    "ZREVRANGE",
    "ZREVRANGEBYSCORE",
    "ZREVRANK",
    "ZSCORE",
    "ZUNION",
    "ZMSCORE",
    "ZUNIONSTORE",
    "SCAN",
    "SSCAN",
    "HSCAN",
    "ZSCAN",
    "XINFO",
    "XADD",
    "XTRIM",
    "XDEL",
    "XRANGE",
    "XREVRANGE",
    "XLEN",
    "XREAD",
    "XGROUP",
    "XREADGROUP",
    "XACK",
    "XCLAIM",
    "XAUTOCLAIM",
    "XPENDING",
    "LATENCY DOCTOR",
    "LATENCY GRAPH",
    "LATENCY HISTORY",
    "LATENCY LATEST",
    "LATENCY RESET",
    "LATENCY HELP",
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

constexpr std::string_view kQuotationMark = "\"";

std::string Quote(std::string_view text) {
  return absl::StrCat(kQuotationMark, text, kQuotationMark);
}

std::string_view Unquote(std::string_view text) {
  if (text.size() < 2) {
    return text;
  }
  if (!absl::StartsWith(text, kQuotationMark) || !absl::EndsWith(text, kQuotationMark)) {
    return text;
  }
  return text.substr(1, text.size() - 2);
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
    msg->payload = kNullBulkString;
    return Status::OK();
  }

  PL_ASSIGN_OR_RETURN(std::string_view payload,
                      decoder->ExtractString(len + kTerminalSequence.size()));
  if (!absl::EndsWith(payload, kTerminalSequence)) {
    return error::InvalidArgument("Bulk string should be terminated by '$0'", kTerminalSequence);
  }
  payload.remove_suffix(kTerminalSequence.size());
  msg->payload = Quote(payload);
  return Status::OK();
}

std::optional<std::string_view> GetCommand(std::string_view payload) {
  auto iter = kCmdList.find(payload);
  if (iter != kCmdList.end()) {
    return *iter;
  }
  return std::nullopt;
}

std::optional<std::string_view> GetCommand(const std::vector<std::string>& payloads) {
  if (payloads.empty()) {
    return {};
  }
  // Search the double-words command first.
  if (payloads.size() >= 2) {
    std::string candidate_cmd =
        absl::AsciiStrToUpper(absl::StrCat(Unquote(payloads[0]), " ", Unquote(payloads[1])));
    auto cmd_opt = GetCommand(candidate_cmd);
    if (cmd_opt.has_value()) {
      return cmd_opt.value();
    }
  }
  std::string candidate_cmd = absl::AsciiStrToUpper(Unquote(payloads[0]));
  return GetCommand(candidate_cmd);
}

bool IsPubMsg(const std::vector<std::string>& payloads) {
  // Published message format is at https://redis.io/topics/pubsub#format-of-pushed-messages
  constexpr size_t kArrayPayloadSize = 3;
  if (payloads.size() < kArrayPayloadSize) {
    return false;
  }
  constexpr std::string_view kMessageStr = "MESSAGE";
  if (absl::AsciiStrToUpper(Unquote(payloads.front())) != kMessageStr) {
    return false;
  }
  return true;
}

// This calls ParseMessage(), which eventually calls ParseArray() and are both recursive functions.
// This is because Array message can include nested array messages.
Status ParseArray(MessageType type, BinaryDecoder* decoder, Message* msg);

Status ParseMessage(MessageType type, BinaryDecoder* decoder, Message* msg) {
  PL_ASSIGN_OR_RETURN(const char type_marker, decoder->ExtractChar());

  switch (type_marker) {
    case kSimpleStringMarker: {
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = Quote(str);
      break;
    }
    case kBulkStringsMarker: {
      PL_RETURN_IF_ERROR(ParseBulkString(decoder, msg));
      break;
    }
    case kErrorMarker: {
      PL_ASSIGN_OR_RETURN(std::string_view str, decoder->ExtractStringUntil(kTerminalSequence));
      msg->payload = Quote(str);
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

  msg->payload = absl::StrCat("[", absl::StrJoin(payloads, ", "), "]");

  // Redis wire protocol said requests are array consisting of bulk strings:
  // https://redis.io/topics/protocol#sending-commands-to-a-redis-server
  if (type == MessageType::kRequest) {
    msg->command = GetCommand(payloads).value_or(std::string_view{});
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
