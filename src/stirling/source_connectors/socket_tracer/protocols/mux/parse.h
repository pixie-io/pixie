#pragma once

#include "src/common/base/base.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mux/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

/* const int8_t Treq = 1; */
/* const int8_t Rreq = -1; */
const int8_t Tdispatch = 2;
/* const int8_t Rdispatch = -2; */

// control messages
/* const int8_t Tdrain = 64; */
/* const int8_t Rdrain = -64; */
/* const int8_t Tping  = 65; */
/* const int8_t Rping  = -65; */

/* const int8_t Tdiscarded = 66; */
/* const int8_t Rdiscarded = -66; */

/* const int8_t Tlease = 67; */

const int8_t Tinit = 68;
const int8_t Rinit = -68;

const int8_t Rerr = -128;

// only used to preserve backwards compatibility
/* const int8_t TdiscardedOld = -62; */
const int8_t RerrOld       = 127;

Status ParseFrame(BinaryDecoder* decoder, message_type_t type, std::string_view* buf, Frame* frame);

}

template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mux::Frame* frame, NoState* /*state*/);

template <>
size_t FindFrameBoundary<mux::Frame>(message_type_t type, std::string_view buf, size_t start_pos, NoState* /*state*/);

}
}
}
