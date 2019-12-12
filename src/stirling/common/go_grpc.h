#pragma once

#include <algorithm>
#include <string>

extern "C" {
#include "src/stirling/bcc_bpf_interface/go_grpc_types.h"
}

namespace pl {
namespace stirling {

struct HTTP2DataEvent {
  HTTP2DataEvent() : attr{}, payload{} {}
  explicit HTTP2DataEvent(const void* data) {
    memcpy(&attr, static_cast<const char*>(data) + offsetof(go_grpc_data_event_t, attr),
           sizeof(go_grpc_data_event_t::data_attr_t));
    payload.assign(static_cast<const char*>(data) + offsetof(go_grpc_data_event_t, data),
                   std::min<uint32_t>(attr.data_len, sizeof(go_grpc_data_event_t::data)));
  }
  go_grpc_data_event_t::data_attr_t attr;
  // TODO(oazizi/yzhao): payload will be copied into ConnectionTracker/DataStream's internal buffer.
  // It appears we should use string_view here.
  std::string payload;
};

}  // namespace stirling
}  // namespace pl
