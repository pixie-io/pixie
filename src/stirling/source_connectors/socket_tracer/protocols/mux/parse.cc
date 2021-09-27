#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {


Status ParseFrame(BinaryDecoder* decoder, message_type_t type, std::string_view* buf, Frame* frame) {
  PL_UNUSED(type);

  if (frame->type == RerrOld || frame->type == Rerr) {
    frame->why = buf->substr(8, buf->length());
  }

  PL_ASSIGN_OR_RETURN(uint16_t tagFirst, decoder->ExtractInt<uint16_t>());
  PL_ASSIGN_OR_RETURN(uint8_t tagLast, decoder->ExtractInt<uint8_t>());
  frame->tag = (tagFirst << 8) | tagLast;

  if (frame->type == Rinit || frame->type == Tinit) {
      // TODO: Add support for reading Tinit and Rinit compression, tls and other parameters
  }
  // TODO: Add support for reading the Rdispatch reply status

  PL_ASSIGN_OR_RETURN(int16_t numCtx, decoder->ExtractInt<int16_t>());
  std::map<std::string, std::map<std::string, std::string>> context;

  for (int i = 0; i < numCtx; i++) {
    PL_ASSIGN_OR_RETURN(size_t ctxKeyLen, decoder->ExtractInt<int16_t>());
    PL_ASSIGN_OR_RETURN(std::string_view ctxKey, decoder->ExtractString(ctxKeyLen));

    PL_ASSIGN_OR_RETURN(size_t ctxValueLen, decoder->ExtractInt<int16_t>());

    std::map<std::string, std::string> unpackedValue;
    if (ctxKey == "com.twitter.finagle.Deadline") {

      PL_ASSIGN_OR_RETURN(int64_t timestamp, decoder->ExtractInt<int64_t>());
      PL_ASSIGN_OR_RETURN(int64_t deadline, decoder->ExtractInt<int64_t>());

      unpackedValue["timestamp"] = std::to_string(timestamp / 1000);
      unpackedValue["deadline"] = std::to_string(deadline / 1000);

    } else if (ctxKey == "com.twitter.finagle.tracing.TraceContext") {

      PL_ASSIGN_OR_RETURN(int64_t spanId, decoder->ExtractInt<int64_t>());
      PL_ASSIGN_OR_RETURN(int64_t parentId, decoder->ExtractInt<int64_t>());
      PL_ASSIGN_OR_RETURN(int64_t traceId, decoder->ExtractInt<int64_t>());
      PL_ASSIGN_OR_RETURN(int64_t flags, decoder->ExtractInt<int64_t>());

      unpackedValue["span id"] = std::to_string(spanId);
      unpackedValue["parent id"] = std::to_string(parentId);
      unpackedValue["trace id"] = std::to_string(traceId);
      unpackedValue["flags"] = std::to_string(flags);

    } else if (ctxKey == "com.twitter.finagle.thrift.ClientIdContext") {

      PL_ASSIGN_OR_RETURN(std::string_view ctxValue, decoder->ExtractString(ctxValueLen));
      unpackedValue["name"] = std::string(ctxValue);

    } else {

      PL_ASSIGN_OR_RETURN(std::string_view ctxValue, decoder->ExtractString(ctxValueLen));
      unpackedValue["length"] = std::to_string(ctxValue.length());

    }

    context.insert({std::string(ctxKey), unpackedValue});
  }

  frame->context = context;

  // TODO: Add dest and dtab parsing here

  return Status::OK();
}

}


template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mux::Frame* frame, NoState* /*state*/) {
  PL_UNUSED(type);
    
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(frame->header_length, decoder.ExtractInt<int32_t>(), return ParseState::kInvalid);
  PL_ASSIGN_OR(frame->type, decoder.ExtractInt<int8_t>(), return ParseState::kInvalid);

  if (frame->header_length > buf->length()) {
    return ParseState::kNeedsMoreData;
  }

  Status status = mux::ParseFrame(&decoder, type, buf, frame);
  PL_UNUSED(status);

  /* if (!status.ok()) { */
  /*   return ParseState::kInvalid; */
  /* } */

  return ParseState::kSuccess;
}

template <>
size_t FindFrameBoundary<mux::Frame>(message_type_t /*type*/, std::string_view /*buf*/,
                                      size_t /*start_pos*/, NoState* /*state*/) {
  // Not implemented.
  return std::string::npos;
}

}
}
}
