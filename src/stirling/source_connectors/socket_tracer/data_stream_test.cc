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

#include "src/stirling/source_connectors/socket_tracer/data_stream.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/metrics.h"
#include "src/stirling/source_connectors/socket_tracer/testing/event_generator.h"
#include "src/stirling/testing/common.h"

namespace px {
namespace stirling {

namespace http = protocols::http;
namespace mysql = protocols::mysql;

using ::testing::IsEmpty;
using ::testing::SizeIs;

using testing::kHTTPIncompleteResp;
using testing::kHTTPReq0;
using testing::kHTTPReq1;
using testing::kHTTPReq2;
using testing::kHTTPResp0;

class DataStreamTest : public ::testing::Test {
 protected:
  DataStreamTest() : event_gen_(&mock_clock_) {}

  void TearDown() override {
    TestOnlyResetMetricsRegistry();
    SocketTracerMetrics::TestOnlyResetProtocolMetrics(kProtocolHTTP, kSSLNone);
    SocketTracerMetrics::TestOnlyResetProtocolMetrics(kProtocolHTTP, kSSLUnspecified);
  }

  std::chrono::steady_clock::time_point now() {
    return testing::NanosToTimePoint(mock_clock_.now());
  }

  testing::MockClock mock_clock_;
  testing::EventGenerator event_gen_;
};

TEST_F(DataStreamTest, LostEvent) {
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req3 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req4 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req5 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);

  // Start off with no lost events.
  stream.AddData(std::move(req0));
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(1));

  // Now add some lost events - should get skipped over.
  PX_UNUSED(req1);  // Lost event.
  stream.AddData(std::move(req2));
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(2));

  // Some more requests, and another lost request (this time undetectable).
  stream.AddData(std::move(req3));
  PX_UNUSED(req4);
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(3));

  // Now the lost event should be detected.
  stream.AddData(std::move(req5));
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(4));

  EXPECT_EQ(
      req1->msg.size() + req4->msg.size(),
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, StuckTemporarily) {
  std::unique_ptr<SocketDataEvent> req0a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() - 10));
  std::unique_ptr<SocketDataEvent> req0b =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() - 10, 10));
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.AddData(std::move(req0a));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], IsEmpty());

  // Remaining data arrives in time, so stuck count never gets high enough to flush events.
  stream.AddData(std::move(req0b));
  stream.AddData(std::move(req1));
  stream.AddData(std::move(req2));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(3));
  EXPECT_EQ(requests[0].req_path, "/index.html");
  EXPECT_EQ(requests[1].req_path, "/foo.html");
  EXPECT_EQ(requests[2].req_path, "/bar.html");

  EXPECT_EQ(
      0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, StuckTooLong) {
  std::unique_ptr<SocketDataEvent> req0a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() - 10));
  std::unique_ptr<SocketDataEvent> req0b =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() - 10, 10));
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.set_current_time(now());
  stream.AddData(std::move(req0a));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], IsEmpty());

  stream.set_current_time(now() + std::chrono::seconds(FLAGS_buffer_expiration_duration_secs));

  // Remaining data does not arrive in time, so stuck recovery will kick in to remove req0a.
  // Then req0b will be noticed as invalid and cleared out as well.
  stream.AddData(std::move(req0b));
  stream.AddData(std::move(req1));
  stream.AddData(std::move(req2));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(2));
  EXPECT_EQ(requests[0].req_path, "/foo.html");
  EXPECT_EQ(requests[1].req_path, "/bar.html");

  EXPECT_EQ(
      kHTTPReq0.length(),
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, PartialMessageRecovery) {
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.AddData(std::move(req0));
  stream.AddData(std::move(req1a));
  PX_UNUSED(req1b);  // Missing event.
  stream.AddData(std::move(req2));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(2));
  EXPECT_EQ(requests[0].req_path, "/index.html");
  EXPECT_EQ(requests[1].req_path, "/bar.html");

  EXPECT_EQ(
      kHTTPReq1.length(),
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, HeadAndMiddleMissing) {
  std::unique_ptr<SocketDataEvent> req0b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req1a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(0, kHTTPReq2.length() / 2));
  std::unique_ptr<SocketDataEvent> req2b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq2.substr(kHTTPReq2.length() / 2, kHTTPReq2.length()));
  protocols::http::StateWrapper state{};

  size_t req0b_size = req0b->msg.size();

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.AddData(std::move(req0b));
  stream.AddData(std::move(req1a));
  PX_UNUSED(req1b);  // Missing event.
  stream.AddData(std::move(req2a));
  stream.AddData(std::move(req2b));

  // The presence of a missing event should trigger the stream to make forward progress.

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(1));
  EXPECT_EQ(requests[0].req_path, "/bar.html");

  EXPECT_EQ(
      req0b_size + kHTTPReq1.length(),
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, LateArrivalPlusMissingEvents) {
  std::unique_ptr<SocketDataEvent> req0a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() / 2));
  std::unique_ptr<SocketDataEvent> req0b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req1a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req1b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  std::unique_ptr<SocketDataEvent> req2a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2.substr(0, kHTTPReq2.length() / 2));
  std::unique_ptr<SocketDataEvent> req2b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq2.substr(kHTTPReq2.length() / 2, kHTTPReq2.length()));
  std::unique_ptr<SocketDataEvent> req3a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() / 2));
  std::unique_ptr<SocketDataEvent> req3b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq0.substr(kHTTPReq0.length() / 2, kHTTPReq0.length()));
  std::unique_ptr<SocketDataEvent> req4a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1.substr(0, kHTTPReq1.length() / 2));
  std::unique_ptr<SocketDataEvent> req4b = event_gen_.InitSendEvent<kProtocolHTTP>(
      kHTTPReq1.substr(kHTTPReq1.length() / 2, kHTTPReq1.length()));
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.AddData(std::move(req0a));
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  ASSERT_THAT(frames[0], IsEmpty());

  // Setting buffer_expiry_timestamp to now to simulate a large delay.
  int buffer_size_limit = 10000;
  auto buffer_expiry_timestamp = now();

  stream.CleanupEvents(buffer_size_limit, buffer_expiry_timestamp);
  auto empty = stream.Empty<http::stream_id_t, http::Message>();
  EXPECT_TRUE(empty);

  stream.AddData(std::move(req0b));
  stream.AddData(std::move(req1a));
  stream.AddData(std::move(req1b));
  PX_UNUSED(req2a);  // Missing event.
  PX_UNUSED(req2b);  // Missing event.
  stream.AddData(std::move(req3a));
  stream.AddData(std::move(req3b));
  stream.AddData(std::move(req4a));
  stream.AddData(std::move(req4b));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(3));
  EXPECT_EQ(requests[0].req_path, "/foo.html");
  EXPECT_EQ(requests[1].req_path, "/index.html");
  EXPECT_EQ(requests[2].req_path, "/foo.html");

  EXPECT_EQ(
      kHTTPReq0.length() + kHTTPReq2.length(),
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

// This test checks that various stats updated on each call ProcessBytesToFrames()
// are updated correctly.
TEST_F(DataStreamTest, Stats) {
  std::unique_ptr<SocketDataEvent> req0 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2bad =
      event_gen_.InitSendEvent<kProtocolHTTP>("This is not a valid HTTP message");
  std::unique_ptr<SocketDataEvent> req3 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0);
  std::unique_ptr<SocketDataEvent> req4 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req5 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req6bad =
      event_gen_.InitSendEvent<kProtocolHTTP>("Another malformed message");
  std::unique_ptr<SocketDataEvent> req7 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.AddData(std::move(req0));
  stream.AddData(std::move(req1));
  stream.AddData(std::move(req2bad));

  EXPECT_EQ(stream.stat_raw_data_gaps(), 0);
  EXPECT_EQ(stream.stat_invalid_frames(), 0);
  EXPECT_EQ(stream.stat_valid_frames(), 0);

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_EQ(frames[0].size(), 2);
  EXPECT_EQ(stream.stat_raw_data_gaps(), 0);
  EXPECT_EQ(stream.stat_invalid_frames(), 1);
  EXPECT_EQ(stream.stat_valid_frames(), 2);

  stream.AddData(std::move(req3));
  PX_UNUSED(req4);  // Skip req4 as missing event.
  stream.AddData(std::move(req5));
  stream.AddData(std::move(req6bad));
  stream.AddData(std::move(req7));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_EQ(frames[0].size(), 5);
  EXPECT_EQ(stream.stat_raw_data_gaps(), 1);
  EXPECT_EQ(stream.stat_invalid_frames(), 2);
  EXPECT_EQ(stream.stat_valid_frames(), 5);
}

TEST_F(DataStreamTest, Stress) {
  constexpr int kIters = 100;

  std::default_random_engine rng(37777);

  // Pack a bunch of requests together.
  std::string data;
  for (int i = 0; i < 100; ++i) {
    data += kHTTPReq0;
  }

  protocols::http::StateWrapper state{};
  // Repeat this randomized test many times.
  for (int iter = 0; iter < kIters; ++iter) {
    DataStream stream;
    stream.set_protocol(kProtocolHTTP);

    // Chop the requests in random ways into a number of events.
    std::string_view d(data);
    std::vector<std::unique_ptr<SocketDataEvent>> events;
    std::uniform_int_distribution<size_t> event_dist(1, 2 * kHTTPReq0.size());
    while (!d.empty()) {
      size_t len = std::min(event_dist(rng), d.size());
      events.push_back(event_gen_.InitSendEvent<kProtocolHTTP>(d.substr(0, len)));
      d.remove_prefix(len);
    }

    // Add the events in shuffled order, and occasionally drop some events.
    std::shuffle(std::begin(events), std::end(events), rng);
    std::uniform_real_distribution<double> probability(0, 1.0);

    for (auto& event : events) {
      double p = probability(rng);
      if (p < 0.01) {
        // Occasionally drop an event (don't do anything inside this if statement).
      } else if (p < 0.02) {
        // Occasionally corrupt the data.
        std::shuffle(const_cast<char*>(event->msg.begin()), const_cast<char*>(event->msg.end()),
                     rng);
        stream.AddData(std::move(event));
      } else if (p < 0.03) {
        // Occasionally reset the stream.
        stream.Reset();
      } else {
        // Add data correctly.
        stream.AddData(std::move(event));
      }
    }

    // Process the events. Here we are looking for any DCHECKS that may fire.
    stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  }
}

TEST_F(DataStreamTest, CannotSwitchType) {
  protocols::http::StateWrapper http_state{};
  DataStream stream;
  stream.set_protocol(kProtocolHTTP);

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest,
                                                                &http_state);

#if DCHECK_IS_ON()
  protocols::mysql::StateWrapper mysql_state{};
  EXPECT_DEATH((stream.ProcessBytesToFrames<mysql::connection_id_t, mysql::Packet>(
                   message_type_t::kRequest, &mysql_state)),
               "ConnTracker cannot change the type it holds during runtime");
#else
  protocols::mysql::StateWrapper mysql_state{};
  EXPECT_THROW((stream.ProcessBytesToFrames<mysql::connection_id_t, mysql::Packet>(
                   message_type_t::kRequest, &mysql_state)),
               std::exception);
#endif
}

TEST_F(DataStreamTest, SpikeCapacityWithLargeDataChunk) {
  int spike_capacity_bytes = 1024;
  int retention_capacity_bytes = 16;
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);
  DataStream stream(spike_capacity_bytes);
  stream.set_protocol(kProtocolHTTP);

  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPIncompleteResp);

  stream.AddData(std::move(resp0));
  stream.AddData(std::move(resp1));
  stream.AddData(std::move(resp2));

  protocols::http::StateWrapper state{};
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kResponse, &state);
  stream.CleanupEvents(retention_capacity_bytes, buffer_expiry_timestamp);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(2));
  EXPECT_EQ(stream.data_buffer().size(), 16);

  // Run ProcessBytesToFrames again to propagate data loss stats.
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kResponse, &state);
  EXPECT_EQ(
      kHTTPIncompleteResp.length() - retention_capacity_bytes,
      SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
  EXPECT_EQ(0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                   .data_loss_bytes.Value());
}

TEST_F(DataStreamTest, SpikeCapacityWithLargeDataChunkAndSSLEnabled) {
  int spike_capacity_bytes = 1024;
  int retention_capacity_bytes = 16;
  auto buffer_expiry_timestamp = now() - std::chrono::seconds(10000);
  DataStream stream(spike_capacity_bytes);
  stream.set_ssl_source(kSSLUnspecified);
  stream.set_protocol(kProtocolHTTP);

  std::unique_ptr<SocketDataEvent> resp0 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp1 = event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPResp0);
  std::unique_ptr<SocketDataEvent> resp2 =
      event_gen_.InitRecvEvent<kProtocolHTTP>(kHTTPIncompleteResp);

  stream.AddData(std::move(resp0));
  stream.AddData(std::move(resp1));
  stream.AddData(std::move(resp2));

  protocols::http::StateWrapper state{};
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kResponse, &state);
  stream.CleanupEvents(retention_capacity_bytes, buffer_expiry_timestamp);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], SizeIs(2));
  EXPECT_EQ(stream.data_buffer().size(), 16);

  // Run ProcessBytesToFrames again to propagate data loss stats.
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kResponse, &state);
  EXPECT_EQ(kHTTPIncompleteResp.length() - retention_capacity_bytes,
            SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLUnspecified)
                .data_loss_bytes.Value());
  EXPECT_EQ(
      0, SocketTracerMetrics::GetProtocolMetrics(kProtocolHTTP, kSSLNone).data_loss_bytes.Value());
}

TEST_F(DataStreamTest, ResyncCausesDuplicateEventBug) {
  // Test to catch regression on a bug. The bug occured when a resync occurs in ParseFrames, leading
  // to an invalid state where the data stream buffer still had data that had already been
  // processed.
  std::unique_ptr<SocketDataEvent> req0a =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(0, kHTTPReq0.length() - 10));
  std::unique_ptr<SocketDataEvent> req0b =
      event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq0.substr(kHTTPReq0.length() - 10, 10));
  std::unique_ptr<SocketDataEvent> req1 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq1);
  std::unique_ptr<SocketDataEvent> req2 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  std::unique_ptr<SocketDataEvent> req3 = event_gen_.InitSendEvent<kProtocolHTTP>(kHTTPReq2);
  protocols::http::StateWrapper state{};

  DataStream stream;
  stream.set_protocol(kProtocolHTTP);
  stream.set_current_time(now());
  stream.AddData(std::move(req0a));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);
  auto frames = stream.Frames<http::stream_id_t, http::Message>();
  EXPECT_THAT(frames[0], IsEmpty());

  stream.set_current_time(now() + std::chrono::seconds(FLAGS_buffer_expiration_duration_secs));

  // Remaining data does not arrive in time, so stuck recovery will kick in to remove req0a.
  // Then req0b will be noticed as invalid and cleared out as well.
  stream.AddData(std::move(req0b));
  stream.AddData(std::move(req1));
  stream.AddData(std::move(req2));

  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);

  stream.AddData(std::move(req3));
  stream.ProcessBytesToFrames<http::stream_id_t, http::Message>(message_type_t::kRequest, &state);

  const auto& requests = stream.Frames<http::stream_id_t, http::Message>()[0];
  ASSERT_THAT(requests, SizeIs(3));
  EXPECT_EQ(requests[0].req_path, "/foo.html");
  EXPECT_EQ(requests[1].req_path, "/bar.html");
  EXPECT_EQ(requests[2].req_path, "/bar.html");
}

}  // namespace stirling
}  // namespace px
