#include "src/stirling/source_connectors/socket_tracer/protocols/redis/parse.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/common/base/base.h"
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

// This list is produced with by:
//   //src/stirling/source_connectors/socket_tracer/protocols/redis:redis_cmds_format_generator
//
//   Read its help message to get the instructions.
const absl::flat_hash_map<std::string_view, std::vector<std::string_view>> kCmdList = {
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
    return iter->first;
  }
  return std::nullopt;
}

std::optional<std::string_view> GetCommand(VectorView<std::string>* payloads) {
  if (payloads->empty()) {
    return std::nullopt;
  }
  // Search the double-words command first.
  if (payloads->size() >= 2) {
    std::string candidate_cmd =
        absl::AsciiStrToUpper(absl::StrCat(Unquote((*payloads)[0]), " ", Unquote((*payloads)[1])));
    auto cmd_opt = GetCommand(candidate_cmd);
    if (cmd_opt.has_value()) {
      payloads->pop_front(2);
      return cmd_opt.value();
    }
  }
  std::string candidate_cmd = absl::AsciiStrToUpper(Unquote(payloads->front()));
  auto cmd_opt = GetCommand(candidate_cmd);
  if (cmd_opt.has_value()) {
    payloads->pop_front(1);
  }
  return cmd_opt;
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

  auto payloads_view = VectorView<std::string>(payloads);

  // Redis wire protocol said requests are array consisting of bulk strings:
  // https://redis.io/topics/protocol#sending-commands-to-a-redis-server
  if (type == MessageType::kRequest) {
    msg->command = GetCommand(&payloads_view).value_or(std::string_view{});
  }

  msg->payload = absl::StrCat("[", absl::StrJoin(payloads_view, ", "), "]");

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
