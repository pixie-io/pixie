#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

ParseState ParseFullFrame(BinaryDecoder* decoder, message_type_t /* type */, std::string_view* buf, Frame* frame) {

  // TODO(oazizi/ddelnano): Simplify this logic when the binary decoder supports reading 24 bit fields
  PL_ASSIGN_OR(uint16_t tagFirst, decoder->ExtractInt<uint16_t>(), return ParseState::kInvalid);
  PL_ASSIGN_OR(uint8_t tagLast, decoder->ExtractInt<uint8_t>(), return ParseState::kInvalid);
  frame->tag = (tagFirst << 8) | tagLast;

  if (frame->type == static_cast<int8_t>(Type::RerrOld) || frame->type == static_cast<int8_t>(Type::Rerr)) {
    size_t why_len = buf->length() - 8;
    PL_ASSIGN_OR(std::string_view why, decoder->ExtractString(why_len), return ParseState::kInvalid);
    frame->why = std::string(why);
    return ParseState::kSuccess;
  }

  if (frame->type == static_cast<int8_t>(Type::Rinit) || frame->type == static_cast<int8_t>(Type::Tinit)) {
      // TODO: Add support for reading Tinit and Rinit compression, tls and other parameters
    return ParseState::kSuccess;
  }
  // TODO: Add support for reading the Rdispatch reply status

  PL_ASSIGN_OR(int16_t numCtx, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);
  std::map<std::string, std::map<std::string, std::string>> context;

  for (int i = 0; i < numCtx; i++) {
    PL_ASSIGN_OR(size_t ctxKeyLen, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);
    PL_ASSIGN_OR(std::string_view ctxKey, decoder->ExtractString(ctxKeyLen), return ParseState::kInvalid);

    PL_ASSIGN_OR(size_t ctxValueLen, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);

    std::map<std::string, std::string> unpackedValue;
    if (ctxKey == "com.twitter.finagle.Deadline") {

      PL_ASSIGN_OR(int64_t timestamp, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t deadline, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);

      unpackedValue["timestamp"] = std::to_string(timestamp / 1000);
      unpackedValue["deadline"] = std::to_string(deadline / 1000);

    } else if (ctxKey == "com.twitter.finagle.tracing.TraceContext") {

      PL_ASSIGN_OR(int64_t spanId, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t parentId, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t traceId, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t flags, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);

      unpackedValue["span id"] = std::to_string(spanId);
      unpackedValue["parent id"] = std::to_string(parentId);
      unpackedValue["trace id"] = std::to_string(traceId);
      unpackedValue["flags"] = std::to_string(flags);

    } else if (ctxKey == "com.twitter.finagle.thrift.ClientIdContext") {

      PL_ASSIGN_OR(std::string_view ctxValue, decoder->ExtractString(ctxValueLen), return ParseState::kInvalid);
      unpackedValue["name"] = std::string(ctxValue);

    } else {

      PL_ASSIGN_OR(std::string_view ctxValue, decoder->ExtractString(ctxValueLen), return ParseState::kInvalid);
      unpackedValue["length"] = std::to_string(ctxValue.length());

    }

    context.insert({std::string(ctxKey), unpackedValue});
  }

  frame->context = context;

  // TODO: Add dest and dtab parsing here

  return ParseState::kSuccess;
}

}


template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mux::Frame* frame, NoState* /*state*/) {
  PL_UNUSED(type);
    
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(frame->header_length, decoder.ExtractInt<int32_t>(), return ParseState::kInvalid);
  if (frame->header_length > buf->length()) {
    return ParseState::kNeedsMoreData;
  }

  PL_ASSIGN_OR(frame->type, decoder.ExtractInt<int8_t>(), return ParseState::kInvalid);
  if (! mux::IsMuxType(frame->type)) {
      return ParseState::kInvalid;
  }

  return mux::ParseFullFrame(&decoder, type, buf, frame);
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
