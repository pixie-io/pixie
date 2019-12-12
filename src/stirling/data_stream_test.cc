#include "src/stirling/data_stream.h"

#include <sys/socket.h>

#include <utility>

#include "src/common/testing/testing.h"
#include "src/stirling/testing/events_fixture.h"

namespace pl {
namespace stirling {

using DataStreamTest = testing::EventsFixture;

TEST_F(DataStreamTest, LostEvent) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req3 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req4 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req5 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);

  std::deque<http::HTTPMessage> requests;

  // Start off with no lost events.
  stream.AddEvent(std::move(req0));
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 1ULL);
  EXPECT_FALSE(stream.IsStuck());

  // Now add some lost events - should get skipped over.
  PL_UNUSED(req1);  // Lost event.
  stream.AddEvent(std::move(req2));
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 2ULL);
  EXPECT_FALSE(stream.IsStuck());

  // Some more requests, and another lost request (this time undetectable).
  stream.AddEvent(std::move(req3));
  PL_UNUSED(req4);
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 3ULL);
  EXPECT_FALSE(stream.IsStuck());

  // Now the lost event should be detected.
  stream.AddEvent(std::move(req5));
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 4ULL);
  EXPECT_FALSE(stream.IsStuck());
}

TEST_F(DataStreamTest, AttemptHTTPReqRecoveryStuckStream) {
  DataStream stream;

  // First request is missing a few bytes from its start.
  std::unique_ptr<SocketDataEvent> req0 =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(5, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req1 = InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);

  stream.AddEvent(std::move(req0));
  stream.AddEvent(std::move(req1));
  stream.AddEvent(std::move(req2));

  std::deque<http::HTTPMessage> requests;

  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 0ULL);

  // Stuck count = 1.
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 0ULL);

  // Stuck count = 2. Should invoke recovery and release two messages.
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  EXPECT_EQ(requests.size(), 2ULL);
}

TEST_F(DataStreamTest, AttemptHTTPRespRecoveryStuckStream) {
  DataStream stream;

  // First response is missing a few bytes from its start.
  std::unique_ptr<SocketDataEvent> resp0 =
      InitSendEvent<kProtocolHTTP>(kHTTPResp0.substr(5, kHTTPResp0.length()));
  std::unique_ptr<SocketDataEvent> resp1 = InitSendEvent<kProtocolHTTP>(kHTTPResp1);
  std::unique_ptr<SocketDataEvent> resp2 = InitSendEvent<kProtocolHTTP>(kHTTPResp2);

  stream.AddEvent(std::move(resp0));
  stream.AddEvent(std::move(resp1));
  stream.AddEvent(std::move(resp2));

  std::deque<http::HTTPMessage> responses;

  responses = stream.ExtractMessages<http::HTTPMessage>(MessageType::kResponse);
  EXPECT_EQ(responses.size(), 0ULL);

  // Stuck count = 1.
  responses = stream.ExtractMessages<http::HTTPMessage>(MessageType::kResponse);
  EXPECT_EQ(responses.size(), 0ULL);

  // Stuck count = 2. Should invoke recovery and release two messages.
  responses = stream.ExtractMessages<http::HTTPMessage>(MessageType::kResponse);
  EXPECT_EQ(responses.size(), 2ULL);
}

TEST_F(DataStreamTest, AttemptHTTPReqRecoveryPartialMessage) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0 = InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2 = InitSendEvent<kProtocolHTTP>(kHTTPReq2);

  stream.AddEvent(std::move(req0));
  stream.AddEvent(std::move(req1a));
  PL_UNUSED(req1b);  // Missing event.
  stream.AddEvent(std::move(req2));

  std::deque<http::HTTPMessage> requests;
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 2ULL);
  EXPECT_EQ(requests[0].http_req_path, "/index.html");
  EXPECT_EQ(requests[1].http_req_path, "/bar.html");
}

TEST_F(DataStreamTest, AttemptHTTPRespRecoveryPartialMessage) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> resp0 = InitSendEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1a =
      InitSendEvent<kProtocolHTTP>(kHTTPResp1.substr(0, kHTTPResp1.length() / 2));
  std::unique_ptr<SocketDataEvent> resp1b =
      InitSendEvent<kProtocolHTTP>(kHTTPResp1.substr(kHTTPResp1.length() / 2, kHTTPResp1.length()));
  std::unique_ptr<SocketDataEvent> resp2 = InitSendEvent<kProtocolHTTP>(kHTTPResp2);

  stream.AddEvent(std::move(resp0));
  stream.AddEvent(std::move(resp1a));
  PL_UNUSED(resp1b);  // Missing event.
  stream.AddEvent(std::move(resp2));

  std::deque<http::HTTPMessage> responses;
  responses = stream.ExtractMessages<http::HTTPMessage>(MessageType::kResponse);
  ASSERT_EQ(responses.size(), 2ULL);
  EXPECT_EQ(responses[0].http_msg_body, "pixie");
  EXPECT_EQ(responses[1].http_msg_body, "bar");
}

TEST_F(DataStreamTest, AttemptHTTPReqRecoveryHeadAndMiddleMissing) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req1a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(0, kHTTPReq2.length() / 2));
  std::unique_ptr<SocketDataEvent> req2b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(kHTTPReq2.length() / 2, kHTTPReq2.length()));

  stream.AddEvent(std::move(req0b));
  stream.AddEvent(std::move(req1a));
  PL_UNUSED(req1b);  // Missing event.
  stream.AddEvent(std::move(req2a));
  stream.AddEvent(std::move(req2b));

  // The presence of a missing event should trigger the stream to make forward progress.
  // Contrast this to AttemptHTTPReqRecoveryStuckStream,
  // where the stream stays stuck for several iterations.

  std::deque<http::HTTPMessage> requests;
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 1ULL);
  EXPECT_EQ(requests[0].http_req_path, "/bar.html");
}

TEST_F(DataStreamTest, AttemptHTTPReqRecoveryAggressiveMode) {
  DataStream stream;

  std::unique_ptr<SocketDataEvent> req0a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() / 2));
  std::unique_ptr<SocketDataEvent> req0b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req1a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(0, kHTTPReq2.length() / 2));
  std::unique_ptr<SocketDataEvent> req2b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(kHTTPReq2.length() / 2, kHTTPReq2.length()));
  std::unique_ptr<SocketDataEvent> req3a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() / 2));
  std::unique_ptr<SocketDataEvent> req3b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req4a =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req4b =
      InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));

  std::deque<http::HTTPMessage> requests;

  stream.AddEvent(std::move(req0a));
  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 0ULL);

  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 0ULL);

  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 0ULL);

  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 0ULL);

  // So many stuck iterations, that we should be in aggressive mode now.
  // Aggressive mode should skip over the first request.
  // In this example, it's a dumb choice, but this is just for test purposes.
  // Normally aggressive mode is meant to force unblock a real unparseable head,
  // but which appears to be at a valid HTTP boundary.

  stream.AddEvent(std::move(req0b));
  stream.AddEvent(std::move(req1a));
  stream.AddEvent(std::move(req1b));
  PL_UNUSED(req2a);  // Missing event.
  PL_UNUSED(req2b);  // Missing event.
  stream.AddEvent(std::move(req3a));
  stream.AddEvent(std::move(req3b));
  stream.AddEvent(std::move(req4a));
  stream.AddEvent(std::move(req4b));

  requests = stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);
  ASSERT_EQ(requests.size(), 3ULL);
  EXPECT_EQ(requests[0].http_req_path, "/foo.html");
  EXPECT_EQ(requests[1].http_req_path, "/index.html");
  EXPECT_EQ(requests[2].http_req_path, "/foo.html");
}

TEST_F(DataStreamTest, CannotSwitchType) {
  DataStream stream;
  stream.ExtractMessages<http::HTTPMessage>(MessageType::kRequest);

#if DCHECK_IS_ON()
  EXPECT_DEATH(stream.ExtractMessages<http2::Frame>(MessageType::kRequest),
               "ConnectionTracker cannot change the type it holds during runtime");
#else
  EXPECT_THROW(stream.ExtractMessages<http2::Frame>(MessageType::kRequest), std::exception);
#endif
}

}  // namespace stirling
}  // namespace pl
