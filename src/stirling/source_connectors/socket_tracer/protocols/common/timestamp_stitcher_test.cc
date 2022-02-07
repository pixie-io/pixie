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

#include <gtest/gtest.h>

#include "src/stirling/source_connectors/socket_tracer/protocols/common/timestamp_stitcher.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http/types.h"

namespace px {
namespace stirling {
namespace protocols {

using http::Message;
using http::Record;

Message HTTPReq0Message() {
  Message msg;
  msg.type = message_type_t::kRequest;
  msg.minor_version = 1;
  msg.headers = {{"Host", "www.pixielabs.ai"},
                 {"Accept", "image/gif, image/jpeg, */*"},
                 {"User-Agent", "Mozilla/5.0 (X11; Linux x86_64)"}};
  msg.req_method = "GET";
  msg.req_path = "/index.html";
  msg.body = "msg0";
  msg.timestamp_ns = 1;
  return msg;
}

Message HTTPReq1Message() {
  Message msg;
  msg.type = message_type_t::kRequest;
  msg.minor_version = 1;
  msg.headers = {{"Host", "www.pixielabs.ai"},
                 {"Accept", "image/gif, image/jpeg, */*"},
                 {"User-Agent", "Mozilla/5.0 (X11; Linux x86_64)"}};
  msg.req_method = "GET";
  msg.req_path = "/foo.html";
  msg.body = "msg1";
  msg.timestamp_ns = 3;
  return msg;
}

Message HTTPReq2Message() {
  Message msg;
  msg.type = message_type_t::kRequest;
  msg.minor_version = 1;
  msg.headers = {{"host", "pixielabs.ai"},
                 {"content-type", "application/x-www-form-urlencoded"},
                 {"content-length", "27"}};
  msg.req_method = "POST";
  msg.req_path = "/test";
  msg.body = "msg2";
  msg.timestamp_ns = 5;
  return msg;
}

Message HTTPResp0Message() {
  Message msg;
  msg.type = message_type_t::kResponse;
  msg.minor_version = 1;
  msg.headers = {{"Content-Type", "foo"}, {"Content-Length", "9"}};
  msg.resp_status = 200;
  msg.resp_message = "OK";
  msg.body = "msg0";
  msg.timestamp_ns = 2;
  return msg;
}

Message HTTPResp1Message() {
  Message msg;
  msg.type = message_type_t::kResponse;
  msg.minor_version = 1;
  msg.headers = {{"Content-Type", "bar"}, {"Content-Length", "21"}};
  msg.resp_status = 200;
  msg.resp_message = "OK";
  msg.body = "msg1";
  msg.timestamp_ns = 4;
  return msg;
}

Message HTTPResp2Message() {
  Message msg;
  msg.type = message_type_t::kResponse;
  msg.minor_version = 1;
  msg.headers = {{"Transfer-Encoding", "chunked"}};
  msg.resp_status = 200;
  msg.resp_message = "OK";
  msg.body = "msg2";
  msg.timestamp_ns = 6;
  return msg;
}

class TimestampStitcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const Message req0 = HTTPReq0Message();
    const Message req1 = HTTPReq1Message();
    const Message req2 = HTTPReq2Message();
    const Message resp0 = HTTPResp0Message();
    const Message resp1 = HTTPResp1Message();
    const Message resp2 = HTTPResp2Message();

    std::vector<Message> reqs{req0, req1, req2};
    std::vector<Message> resps{resp0, resp1, resp2};

    for (int i = 0; i < 100; ++i) {
      auto req = reqs[i % 3];
      auto resp = resps[i % 3];
      req.timestamp_ns = 2 * i + 1;
      resp.timestamp_ns = 2 * i + 2;
      req_stream_.push_back(req);
      resp_stream_.push_back(resp);
    }
  }

  auto GetReqStream() { return req_stream_; }
  auto GetRespStream() { return resp_stream_; }

 private:
  std::deque<Message> req_stream_;
  std::deque<Message> resp_stream_;
};

TEST_F(TimestampStitcherTest, BasicSequentialMatching) {
  auto req_messages = GetReqStream();
  auto resp_messages = GetRespStream();

  auto records = StitchMessagesWithTimestampOrder<Record>(&req_messages, &resp_messages);
  EXPECT_EQ(records.records.size(), 100);
}

TEST_F(TimestampStitcherTest, MissingOneReq) {
  auto req_messages = GetReqStream();
  auto resp_messages = GetRespStream();

  // Missing req0;
  req_messages.erase(req_messages.begin() + 5);

  auto records = StitchMessagesWithTimestampOrder<Record>(&req_messages, &resp_messages);
  EXPECT_EQ(records.records.size(), 99);
}

TEST_F(TimestampStitcherTest, MissingOneResp) {
  auto req_messages = GetReqStream();
  auto resp_messages = GetRespStream();

  // Missing resp0;
  resp_messages.erase(resp_messages.begin() + 5);

  auto records = StitchMessagesWithTimestampOrder<Record>(&req_messages, &resp_messages);
  EXPECT_EQ(records.records.size(), 99);
}

// TODO(chengruizhe): This test induces a mismatch in the timestamp stitcher. Update the test once
// HTTP stitching is more robust.
TEST_F(TimestampStitcherTest, MissingOneReqFollowedByOneResp) {
  auto req_messages = GetReqStream();
  auto resp_messages = GetRespStream();

  // Missing req1;
  req_messages.erase(req_messages.begin() + 6);

  // Missing resp0;
  resp_messages.erase(resp_messages.begin() + 5);

  auto records = StitchMessagesWithTimestampOrder<Record>(&req_messages, &resp_messages);

  // This are supposed to be 98 records. We currently have 99 due to a mismatch.
  EXPECT_EQ(records.records.size(), 99);
}

}  // namespace protocols
}  // namespace stirling
}  // namespace px
