#pragma once

#include <string>

#include "src/common/base/base.h"
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/redis/types.h"

namespace pl {
namespace stirling {
namespace protocols {
namespace redis {

// Formats an the payloads of an array message according to its type type, and writes the result
// to the input message result argument.
void FormatArrayMessage(MessageType type, VectorView<std::string> payloads_view, Message* msg);

}  // namespace redis
}  // namespace protocols
}  // namespace stirling
}  // namespace pl
