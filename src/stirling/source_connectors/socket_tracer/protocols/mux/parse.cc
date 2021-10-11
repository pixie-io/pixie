#include "src/stirling/source_connectors/socket_tracer/protocols/mux/parse.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mux {

ParseState ParseFullFrame(BinaryDecoder* decoder, message_type_t /* type */, std::string_view* /*buf*/, Frame* frame) {

  auto remaining_bytes = frame->length;
  // TODO(oazizi/ddelnano): Simplify this logic when the binary decoder supports reading 24 bit fields
  PL_ASSIGN_OR(uint16_t tag_first, decoder->ExtractInt<uint16_t>(), return ParseState::kInvalid);
  PL_ASSIGN_OR(uint8_t tag_last, decoder->ExtractInt<uint8_t>(), return ParseState::kInvalid);
  frame->tag = (tag_first << 8) | tag_last;
  // Account for mux type and tag
  remaining_bytes -= 4;

  if (frame->type == static_cast<int8_t>(Type::kRerrOld) || frame->type == static_cast<int8_t>(Type::kRerr)) {
    PL_ASSIGN_OR(std::string_view why, decoder->ExtractString(remaining_bytes), return ParseState::kInvalid);
    frame->why = std::string(why);
    return ParseState::kSuccess;
  }

  if (frame->type == static_cast<int8_t>(Type::kRinit) || frame->type == static_cast<int8_t>(Type::kTinit)) {
      // TODO: Add support for reading Tinit and Rinit compression, tls and other parameters
    if (! decoder->ExtractString(remaining_bytes).ok()) return ParseState::kInvalid;

    return ParseState::kSuccess;
  }

  if (frame->type == static_cast<int8_t>(Type::kRdispatch)) {
      PL_ASSIGN_OR(frame->reply_status, decoder->ExtractInt<uint8_t>(), return ParseState::kInvalid);
      remaining_bytes -= 1;
  }

  PL_ASSIGN_OR(int16_t num_ctx, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);
  remaining_bytes -= 2;
  std::map<std::string, std::map<std::string, std::string>> context;

  for (int i = 0; i < num_ctx; i++) {
    PL_ASSIGN_OR(size_t ctx_key_len, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);
    remaining_bytes -= 2;

    PL_ASSIGN_OR(std::string_view ctx_key, decoder->ExtractString(ctx_key_len), return ParseState::kInvalid);
    remaining_bytes -= ctx_key_len;

    PL_ASSIGN_OR(size_t ctx_value_len, decoder->ExtractInt<int16_t>(), return ParseState::kInvalid);
    remaining_bytes -= 2;

    std::map<std::string, std::string> unpacked_value;
    if (ctx_key == "com.twitter.finagle.Deadline") {

      PL_ASSIGN_OR(int64_t timestamp, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t deadline, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);

      unpacked_value["timestamp"] = std::to_string(timestamp / 1000);
      unpacked_value["deadline"] = std::to_string(deadline / 1000);

      remaining_bytes -= 16;

    } else if (ctx_key == "com.twitter.finagle.tracing.TraceContext") {

      PL_ASSIGN_OR(int64_t span_id, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t parent_id, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t trace_id, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);
      PL_ASSIGN_OR(int64_t flags, decoder->ExtractInt<int64_t>(), return ParseState::kInvalid);

      unpacked_value["span id"] = std::to_string(span_id);
      unpacked_value["parent id"] = std::to_string(parent_id);
      unpacked_value["trace id"] = std::to_string(trace_id);
      unpacked_value["flags"] = std::to_string(flags);

      remaining_bytes -= 32;

    } else if (ctx_key == "com.twitter.finagle.thrift.ClientIdContext") {

      PL_ASSIGN_OR(std::string_view ctx_value, decoder->ExtractString(ctx_value_len), return ParseState::kInvalid);
      unpacked_value["name"] = std::string(ctx_value);

      remaining_bytes -= ctx_value_len;

    } else {

      PL_ASSIGN_OR(std::string_view ctx_value, decoder->ExtractString(ctx_value_len), return ParseState::kInvalid);
      unpacked_value["length"] = std::to_string(ctx_value.length());

      remaining_bytes -= ctx_value_len;
    }

    context.insert({std::string(ctx_key), unpacked_value});
  }

  frame->context = std::move(context);

  // TODO: Add dest and dtab parsing here
  if (! decoder->ExtractString(remaining_bytes).ok()) return ParseState::kInvalid;

  return ParseState::kSuccess;
}

}


template <>
ParseState ParseFrame(message_type_t type, std::string_view* buf, mux::Frame* frame, NoState* /*state*/) {
    
  BinaryDecoder decoder(*buf);

  PL_ASSIGN_OR(frame->length, decoder.ExtractInt<int32_t>(), return ParseState::kInvalid);
  if (frame->length > buf->length()) {
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
