#include "src/stirling/pgsql/parse.h"

#include <optional>
#include <string>
#include <utility>

#include <absl/strings/ascii.h>
#include <magic_enum.hpp>

#include "src/common/base/base.h"
#include "src/stirling/pgsql/binary_decoder.h"

namespace pl {
namespace stirling {
namespace pgsql {

ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg) {
  constexpr int kMinMsgLen = 1 + sizeof(int32_t);
  if (buf->size() < kMinMsgLen) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder decoder(*buf);
  msg->tag = static_cast<Tag>(decoder.ExtractChar());
  msg->len = decoder.ExtractInteger<int32_t>();
  constexpr int kLenFieldLen = 4;
  if (msg->len < kLenFieldLen) {
    // Len includes the len field itself, so its value cannot be less than the length of the field.
    return ParseState::kInvalid;
  }
  const size_t str_len = msg->len - 4;
  if (decoder.BufSize() < str_len) {
    return ParseState::kNeedsMoreData;
  }
  // Len includes the length field itself (int32_t), so the payload needs to exclude 4 bytes.
  msg->payload = std::string(decoder.ExtractString(str_len));
  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

ParseState ParseStartupMessage(std::string_view* buf, StartupMessage* msg) {
  if (buf->size() < StartupMessage::kMinLen) {
    return ParseState::kNeedsMoreData;
  }

  BinaryDecoder decoder(*buf);

  msg->len = decoder.ExtractInteger<int32_t>();
  msg->proto_ver = {.major = decoder.ExtractInteger<int16_t>(),
                    .minor = decoder.ExtractInteger<int16_t>()};

  const size_t kHeaderSize = 2 * sizeof(int32_t);

  if (decoder.BufSize() < msg->len - kHeaderSize) {
    return ParseState::kNeedsMoreData;
  }

  while (!decoder.Empty()) {
    std::string_view name = decoder.ExtractStringUtil('\0');
    if (name.empty()) {
      // Each name or value is terminated by '\0'. And all name value pairs are terminated by an
      // additional '\0'.
      //
      // Extracting an empty name means we are at the end of the string.
      break;
    }
    std::string_view value = decoder.ExtractStringUtil('\0');
    if (value.empty()) {
      return ParseState::kInvalid;
    }
    msg->nvs.push_back(NV{std::string(name), std::string(value)});
  }
  *buf = decoder.Buf();
  return ParseState::kSuccess;
}

namespace {

using MsgDeqIter = std::deque<RegularMessage>::iterator;

void AdvanceIterBeyondTimestamp(MsgDeqIter* start, const MsgDeqIter& end, uint64_t ts) {
  while (*start != end && (*start)->timestamp_ns < ts) {
    ++(*start);
  }
}

MsgDeqIter FindTag(MsgDeqIter begin, MsgDeqIter end, Tag tag) {
  for (auto iter = begin; iter != end; ++iter) {
    if (iter->tag == tag) {
      return iter;
    }
  }
  return end;
}

}  // namespace

// Given the input as the payload of a kRowDesc message, returns a list of column name.
// Row description format:
// | int16 field count |
// | Field description |
// ...
// Field description format:
// | string name | int32 table ID | int16 column number | int32 type ID | int16 type size |
// | int32 type modifier | int16 format code (text|binary) |
std::vector<std::string_view> ParseRowDesc(std::string_view row_desc) {
  std::vector<std::string_view> res;

  BinaryDecoder decoder(row_desc);
  const int16_t field_count = decoder.ExtractInteger<int16_t>();
  for (int i = 0; i < field_count; ++i) {
    std::string_view col_name = decoder.ExtractStringUtil('\0');

    if (col_name.empty()) {
      // Empty column name is invalid. Just put all remaining data as another name and return.
      VLOG(1) << "Encounter an empty column name on the column " << i;
      res.push_back(decoder.Buf());
      return res;
    }
    res.push_back(col_name);

    constexpr size_t kFieldDescSize = 3 * sizeof(int32_t) + 3 * sizeof(int16_t);
    if (decoder.BufSize() < kFieldDescSize) {
      VLOG(1) << absl::Substitute("Not enough data for parsing, needs $0 bytes, got $1",
                                  kFieldDescSize, decoder.BufSize());
      return res;
    }
    // Just make sure to discard.
    decoder.Discard(kFieldDescSize);
  }
  return res;
}

std::vector<std::optional<std::string_view>> ParseDataRow(std::string_view data_row) {
  std::vector<std::optional<std::string_view>> res;

  BinaryDecoder decoder(data_row);
  const int16_t field_count = decoder.ExtractInteger<int16_t>();

  for (int i = 0; i < field_count; ++i) {
    if (decoder.BufSize() < sizeof(int32_t)) {
      VLOG(1) << "Not enough data";
      return res;
    }
    // The length of the column value, in bytes (this count does not include itself). Can be zero.
    // As a special case, -1 indicates a NULL column value. No value bytes follow in the NULL case.
    auto value_len = decoder.ExtractInteger<int32_t>();
    constexpr int kNullValLen = -1;
    if (value_len == kNullValLen) {
      res.push_back(std::nullopt);
      continue;
    }
    if (value_len == 0) {
      res.push_back({});
      continue;
    }
    if (decoder.BufSize() < static_cast<size_t>(value_len)) {
      VLOG(1) << "Not enough data, copy the rest of data";
      value_len = decoder.BufSize();
    }
    res.push_back(decoder.ExtractString(value_len));
  }
  return res;
}

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps) {
  std::vector<pgsql::Record> records;
  auto req_iter = reqs->begin();
  auto resp_iter = resps->begin();
  // PostgreSQL query mode:
  //   In-order mode: where one query (one regular message) is followed one response (possibly with
  //   multiple regular messages).
  //   The code now can handle this mode.
  //
  //   Batch mode: Multiple queries are batched into a list, and sent to server; the responses are
  //   sent back in the same order as their corresponding queries.
  //   The code now can handle this mode.
  //
  //   Pipeline mode: Seem supported in PostgreSQL.
  //   https://2ndquadrant.github.io/postgres/libpq-batch-mode.html
  //   mentions pipelining. But the details are not clear yet.
  //
  // TODO(yzhao): Research batch and pipeline mode and confirm their behaviors.
  while (req_iter != reqs->end() && resp_iter != resps->end()) {
    // First advance response iterator to be at or later than the request's time stamp.
    AdvanceIterBeyondTimestamp(&resp_iter, resps->end(), req_iter->timestamp_ns);

    // TODO(yzhao): Use a map to encode request type and the actions to find the response.
    // So we can get rid of the switch statement. That also include AdvanceIterBeyondTimestamp()
    // into the handler functions, such that the logic is more grouped.
    switch (req_iter->tag) {
      // NOTE: kPasswd message is a response by client to the authenticate request.
      // But it was still classified as request to server from client, according to our model.
      // And we just ignore such message, and kAuth message as well.
      case Tag::kPasswd:
        // Ignore auth response.
        ++req_iter;
        break;
      case Tag::kQuery:
        resp_iter = FindTag(resp_iter, resps->end(), Tag::kRowDesc);
        if (resp_iter == resps->end()) {
          // Not found, skip current request.
          ++req_iter;
        } else {
          resp_iter->payload = absl::StrJoin(ParseRowDesc(resp_iter->payload), ",");
          resp_iter->len = sizeof(int32_t) + resp_iter->payload.size();
          records.push_back({std::move(*req_iter), std::move(*resp_iter)});
          ++req_iter;
          ++resp_iter;
        }
        break;
      default:
        // By default for any message with unhandled or invalid tag, ignore and continue.
        // TODO(yzhao): Revise based on feedbacks.
        ++req_iter;
        LOG(WARNING) << "Unhandled or invalid tag: " << static_cast<char>(req_iter->tag);
        break;
    }
  }
  reqs->erase(reqs->begin(), req_iter);
  resps->erase(resps->begin(), resp_iter);
  return {records, 0};
}

}  // namespace pgsql

template <>
ParseState ParseFrame(MessageType type, std::string_view* buf, pgsql::RegularMessage* frame) {
  PL_UNUSED(type);

  std::string_view buf_copy = *buf;
  pgsql::StartupMessage startup_msg = {};
  if (ParseStartupMessage(&buf_copy, &startup_msg) == ParseState::kSuccess &&
      !startup_msg.nvs.empty()) {
    // Ignore startup message, but remove it from the buffer.
    *buf = buf_copy;
  }
  return ParseRegularMessage(buf, frame);
}

template <>
size_t FindFrameBoundary<pgsql::RegularMessage>(MessageType type, std::string_view buf,
                                                size_t start) {
  PL_UNUSED(type);
  for (; start < buf.size(); ++start) {
    constexpr int kPGSQLMsgTrialCount = 10;
    auto tmp_buf = buf.substr(start);
    pgsql::RegularMessage msg = {};
    bool parse_succeeded = true;
    for (int i = 0; i < kPGSQLMsgTrialCount; ++i) {
      if (pgsql::ParseRegularMessage(&tmp_buf, &msg) != ParseState::kSuccess) {
        parse_succeeded = false;
        break;
      }
    }
    if (parse_succeeded) {
      return start;
    }
  }
  return std::string_view::npos;
}

RecordsWithErrorCount<pgsql::Record> ProcessFrames(std::deque<pgsql::RegularMessage>* reqs,
                                                   std::deque<pgsql::RegularMessage>* resps,
                                                   NoState* /*state*/) {
  return pgsql::ProcessFrames(reqs, resps);
}

}  // namespace stirling
}  // namespace pl
