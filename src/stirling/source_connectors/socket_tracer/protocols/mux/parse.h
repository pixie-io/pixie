#pragma once

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

ParseState ParseFullFrame(BinaryDecoder* decoder, message_type_t type, std::string_view* buf, Frame* frame);

}

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mux::Frame* frame, NoState* /*state*/);

template <>
size_t FindFrameBoundary<mux::Frame>(message_type_t type, std::string_view buf, size_t start_pos, NoState* /*state*/);

}
}
}
