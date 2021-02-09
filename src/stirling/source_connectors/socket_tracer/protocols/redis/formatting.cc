#include "src/stirling/source_connectors/socket_tracer/protocols/redis/formatting.h"

#include <vector>

#include <absl/container/flat_hash_map.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/redis/cmd_args.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

namespace {

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

// Holds the command name and the description of its arguments.
struct CmdAndArgs {
  const std::string_view name;
  const CmdArgs* args;
};

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

// Returns a JSON string that formats the input arguments as a JSON array.
std::string FormatAsJSONArray(VectorView<std::string> args) {
  std::vector<std::string_view> args_copy = {args.begin(), args.end()};
  return utils::ToJSONString(args_copy);
}

}  // namespace

// Redis wire protocol said requests are array consisting of bulk strings:
// https://redis.io/topics/protocol#sending-commands-to-a-redis-server
void FormatArrayMessage(MessageType type, VectorView<std::string> payloads_view, Message* msg) {
  if (type == MessageType::kRequest) {
    std::optional<CmdAndArgs> cmd_and_args_opt = GetCmdAndArgs(&payloads_view);
    if (cmd_and_args_opt.has_value()) {
      msg->command = cmd_and_args_opt.value().name;
      auto payload_or = cmd_and_args_opt.value().args->FmtArgs(payloads_view);
      if (payload_or.ok()) {
        msg->payload = payload_or.ConsumeValueOrDie();
      } else {
        msg->payload = FormatAsJSONArray(payloads_view);
      }
    } else {
      msg->payload = FormatAsJSONArray(payloads_view);
    }
  } else {
    msg->payload = FormatAsJSONArray(payloads_view);
  }
}

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
