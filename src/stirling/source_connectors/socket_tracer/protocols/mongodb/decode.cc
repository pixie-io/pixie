/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libbson-1.0/bson.h>

#include <rapidjson/document.h>
#include <string>

#include "src/common/base/base.h"
#include "src/common/base/utils.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/decode.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/mongodb/types.h"
#include "src/stirling/utils/binary_decoder.h"

namespace px {
namespace stirling {
namespace protocols {
namespace mongodb {

ParseState ProcessOpMsg(BinaryDecoder* decoder, Frame* frame) {
  PX_ASSIGN_OR(uint32_t flag_bits, decoder->ExtractLEInt<uint32_t>(), return ParseState::kInvalid);

  // Find relevant flag bit information and ensure remaining bits are not set.
  // Bits 0-15 are required and bits 16-31 are optional.
  frame->checksum_present = (flag_bits & kChecksumBitmask) == kChecksumBitmask;
  frame->more_to_come = (flag_bits & kMoreToComeBitmask) == kMoreToComeBitmask;
  frame->exhaust_allowed = (flag_bits & kExhaustAllowedBitmask) == kExhaustAllowedBitmask;
  if (flag_bits & kRequiredUnsetBitmask) {
    return ParseState::kInvalid;
  }

  // Determine the number of checksum bytes in the buffer.
  const size_t checksum_bytes = (frame->checksum_present) ? 4 : 0;

  // Get the section(s) data from the buffer.
  auto all_sections_length = frame->length - kHeaderAndFlagSize - checksum_bytes;
  while (all_sections_length > 0) {
    mongodb::Section section;
    PX_ASSIGN_OR(section.kind, decoder->ExtractLEInt<uint8_t>(), return ParseState::kInvalid);
    // Length of the current section still remaining in the buffer.
    int32_t remaining_section_length = 0;

    if (static_cast<SectionKind>(section.kind) == SectionKind::kSectionKindZero) {
      // Check the length but don't extract it since the later logic requires the buffer to retain
      // it.
      section.length = utils::LEndianBytesToInt<int32_t, 4>(decoder->Buf());
      if (section.length < kSectionLengthSize) {
        return ParseState::kInvalid;
      }
      remaining_section_length = section.length;

    } else if (static_cast<SectionKind>(section.kind) == SectionKind::kSectionKindOne) {
      PX_ASSIGN_OR(section.length, decoder->ExtractLEInt<uint32_t>(), return ParseState::kInvalid);
      if (section.length < kSectionLengthSize) {
        return ParseState::kInvalid;
      }

      // Get the sequence identifier (command argument).
      PX_ASSIGN_OR(std::string_view seq_identifier, decoder->ExtractStringUntil('\0'),
                   return ParseState::kInvalid);
      // Make sure the sequence identifier is a valid OP_MSG kind 1 command argument.
      if (seq_identifier != "documents" && seq_identifier != "updates" &&
          seq_identifier != "deletes") {
        return ParseState::kInvalid;
      }
      remaining_section_length =
          section.length - kSectionLengthSize - seq_identifier.length() - kSectionKindSize;

    } else {
      return ParseState::kInvalid;
    }

    // Extract the document(s) from the section and convert it from type BSON to a JSON string.
    while (remaining_section_length > 0) {
      // We can't extract the length bytes since bson_new_from_data() expects those bytes in
      // the data as well as the expected length in another parameter.
      auto document_length = utils::LEndianBytesToInt<int32_t, 4>(decoder->Buf());
      if (document_length > kMaxBSONObjSize) {
        return ParseState::kInvalid;
      }

      PX_ASSIGN_OR(auto section_body, decoder->ExtractString<uint8_t>(document_length),
                   return ParseState::kInvalid);

      // Check if section_body contains an empty document.
      if (section_body.length() == kSectionLengthSize) {
        section.documents.push_back("");
        remaining_section_length -= document_length;
        continue;
      }

      bson_t* bson_doc = bson_new_from_data(section_body.data(), document_length);
      DEFER(bson_destroy(bson_doc));
      if (bson_doc == NULL) {
        return ParseState::kInvalid;
      }
      char* json = bson_as_canonical_extended_json(bson_doc, NULL);
      DEFER(bson_free(json));
      if (json == NULL) {
        return ParseState::kInvalid;
      }

      // Find the type of command argument from the kind 0 section.
      if (static_cast<SectionKind>(section.kind) == SectionKind::kSectionKindZero) {
        rapidjson::Document doc;
        doc.Parse(json);

        // The type of all request commands and the response to all find command requests
        // will always be the first key.
        auto op_msg_type = doc.MemberBegin()->name.GetString();
        if ((op_msg_type == kInsert || op_msg_type == kDelete || op_msg_type == kUpdate ||
             op_msg_type == kFind || op_msg_type == kCursor)) {
          frame->op_msg_type = op_msg_type;

        } else if (op_msg_type == kHello || op_msg_type == kIsMaster ||
                   op_msg_type == kIsMasterAlternate) {
          // The frame is a handshaking message.
          frame->op_msg_type = op_msg_type;
          frame->is_handshake = true;

        } else {
          // The frame is a response message, find the "ok" key and its value.
          auto itr = doc.FindMember("ok");
          if (itr == doc.MemberEnd()) {
            return ParseState::kInvalid;
          }

          if (itr->value.IsObject()) {
            frame->op_msg_type =
                absl::Substitute("ok: {$0: $1}", itr->value.MemberBegin()->name.GetString(),
                                 itr->value.MemberBegin()->value.GetString());
          } else if (itr->value.IsNumber()) {
            frame->op_msg_type = absl::Substitute("ok: $0", std::to_string(itr->value.GetInt()));
          }
        }
      }

      section.documents.push_back(json);
      remaining_section_length -= document_length;
    }
    frame->sections.push_back(section);
    all_sections_length -= (section.length + kSectionKindSize);
  }

  // Get the checksum data, if necessary.
  if (frame->checksum_present) {
    PX_ASSIGN_OR(frame->checksum, decoder->ExtractLEInt<uint32_t>(), return ParseState::kInvalid);
  }
  return ParseState::kSuccess;
}

ParseState ProcessPayload(BinaryDecoder* decoder, Frame* frame) {
  Type frame_type = static_cast<Type>(frame->op_code);
  switch (frame_type) {
    case Type::kOPMsg:
      return ProcessOpMsg(decoder, frame);
    case Type::kOPCompressed:
      return ParseState::kIgnored;
    case Type::kReserved:
      return ParseState::kIgnored;
    default:
      return ParseState::kInvalid;
  }
  return ParseState::kSuccess;
}

}  // namespace mongodb
}  // namespace protocols
}  // namespace stirling
}  // namespace px
