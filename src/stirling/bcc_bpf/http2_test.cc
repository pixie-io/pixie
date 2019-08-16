#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "src/stirling/bcc_bpf/socket_trace.h"
}
#include "src/common/base/const_types.h"

extern "C" {
// The following helper functions must be before include http2.h, so update_http2_frame_offset()
// can be tested with the fake impls.
#include "src/stirling/bcc_bpf/bpf_helper_test_impl.h"
#include "src/stirling/bcc_bpf/http2.h"
}

TEST(HTTP2FrameParsingTest, update_http2_frame_offset) {
  conn_info_t conn_info = {};
  conn_info.wr_next_http2_frame_offset = 0;
  {
    pl::ConstStrView buf(
        "\x0\x0\x0\x3"
        "12345");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(3, conn_info.wr_next_http2_frame_offset);
  }
  {
    pl::ConstStrView buf(
        "123"
        "\x0\x0\x0\x2"
        "12345");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(2, conn_info.wr_next_http2_frame_offset);
  }
  {
    pl::ConstStrView buf("1");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(1, conn_info.wr_next_http2_frame_offset);
  }
  {
    pl::ConstStrView buf("1");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(0, conn_info.wr_next_http2_frame_offset);
  }
}
