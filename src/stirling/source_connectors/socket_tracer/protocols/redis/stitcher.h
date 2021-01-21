#pragma once

#include <deque>
#include <limits>
#include <utility>
#include <vector>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {

template <>
inline RecordsWithErrorCount<redis::Record> StitchFrames(std::deque<redis::Message>* req_messages,
                                                         std::deque<redis::Message>* resp_messages,
                                                         NoState* /* state */) {
  // NOTE: This cannot handle Redis pipelining if there is any missing message.
  // See https://redis.io/topics/pipelining for Redis pipelining.
  //
  // This is copied from StitchMessagesWithTimestampOrder() in timestamp_stitcher.h.
  // With additionally always pushing published messages into records with a synthesized request.
  // See below.

  std::vector<redis::Record> records;

  redis::Record record;
  record.req.timestamp_ns = 0;
  record.resp.timestamp_ns = 0;

  redis::Message dummy_message;
  dummy_message.timestamp_ns = std::numeric_limits<int64_t>::max();

  auto req_iter = req_messages->begin();
  auto resp_iter = resp_messages->begin();
  while (resp_iter != resp_messages->end()) {
    redis::Message& req = (req_iter == req_messages->end()) ? dummy_message : *req_iter;
    redis::Message& resp = (resp_iter == resp_messages->end()) ? dummy_message : *resp_iter;

    // This if block is the added code to StitchMessagesWithTimestampOrder().
    // For Redis pub/sub, published messages have no corresponding `requests`, therefore we
    // forcefully turn them into records without requests.
    if (resp.is_published_message) {
      // Synthesize the request message.
      record.req.timestamp_ns = resp.timestamp_ns;
      constexpr std::string_view kPubPushCmd = "PUSH PUB";
      record.req.command = kPubPushCmd;

      record.resp = std::move(resp);

      records.push_back(std::move(record));
      ++resp_iter;
      continue;
    }

    if (req.timestamp_ns < resp.timestamp_ns) {
      record.req = std::move(req);
      ++req_iter;
    } else {
      if (record.req.timestamp_ns != 0) {
        record.resp = std::move(resp);

        records.push_back(std::move(record));

        record.req.timestamp_ns = 0;
        record.resp.timestamp_ns = 0;
      }
      ++resp_iter;
    }
  }

  req_messages->erase(req_messages->begin(), req_iter);
  resp_messages->erase(resp_messages->begin(), resp_iter);

  return {std::move(records), 0};
}

}  // namespace protocols
}  // namespace stirling
}  // namespace pl
