#include <arpa/inet.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

extern "C" {
#include "src/stirling/bcc_bpf/socket_trace.h"
}
#include "src/common/base/types.h"

extern "C" {
// The following helper functions must be before include http2.h, so update_http2_frame_offset()
// can be tested with the fake impls.
#include "src/stirling/bcc_bpf/bpf_helper_test_impl.h"
#include "src/stirling/bcc_bpf/grpc.h"
#include "src/stirling/bcc_bpf/http2.h"
}

TEST(HTTP2FrameParsingTest, update_http2_frame_offset) {
  conn_info_t conn_info = {};
  conn_info.wr_next_http2_frame_offset = 0;
  {
    std::string_view buf = ConstStringView(
        "\x0\x0\x3"
        "123456");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(3, conn_info.wr_next_http2_frame_offset);
  }
  {
    std::string_view buf = ConstStringView(
        "123"
        "\x0\x0\x2"
        "123456");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(2, conn_info.wr_next_http2_frame_offset);
  }
  {
    std::string_view buf = ConstStringView("1");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(1, conn_info.wr_next_http2_frame_offset);
  }
  {
    std::string_view buf = ConstStringView("1");
    update_http2_frame_offset(TrafficDirection::kEgress, buf.data(), buf.size(), &conn_info);
    EXPECT_EQ(0, conn_info.wr_next_http2_frame_offset);
  }
}

TEST(HTTP2FrameParsingTest, looks_like_grpc_req_http2_headers_frame) {
  const std::string_view buf =
      ConstStringView("\x00\x00\x33\x01\x04\x00\x00\x00\x13\x86\x83\xC6\x0F");
  EXPECT_TRUE(looks_like_grpc_req_http2_headers_frame(buf.data(), buf.size()));
}
