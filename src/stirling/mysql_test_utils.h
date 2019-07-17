#pragma once
#include <string>
#include <utility>
#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/stirling/socket_trace_event_type.h"

namespace pl {
namespace stirling {
namespace testutils {
std::string GenPacket(int packet_num, const std::string& msg);

std::string GenRequest(const ConstStrView& command, const std::string& msg);

std::string GenErr(const std::string& msg);

std::string GenOk(const std::string& msg);

}  // namespace testutils
}  // namespace stirling
}  // namespace pl
