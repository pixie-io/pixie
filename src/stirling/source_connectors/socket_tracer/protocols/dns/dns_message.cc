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

#include "src/stirling/source_connectors/socket_tracer/protocols/dns/dns_message.h"

namespace px {
namespace stirling {
namespace protocols {
namespace dns {

namespace {

constexpr size_t kHeaderSize = 12;
constexpr size_t kQuestionFixedSize = 4;
constexpr size_t kRecordFixedSize = 10;
constexpr size_t kMaxNameLength = 255;
constexpr int kMaxPointerHops = 64;

uint16_t ReadU16(std::string_view buf, size_t pos) {
  return static_cast<uint16_t>((static_cast<uint8_t>(buf[pos]) << 8) |
                               static_cast<uint8_t>(buf[pos + 1]));
}

bool ReadName(std::string_view buf, size_t pos, size_t* in_place, std::string* out) {
  size_t p = pos;
  bool jumped = false;
  int hops = 0;
  size_t decoded_len = 0;
  if (out != nullptr) {
    out->clear();
  }
  while (true) {
    if (p >= buf.size()) {
      return false;
    }
    const uint8_t c = static_cast<uint8_t>(buf[p]);
    if ((c & 0xC0) == 0xC0) {
      if (p + 1 >= buf.size()) {
        return false;
      }
      if (!jumped) {
        *in_place = p + 2 - pos;
        jumped = true;
      }
      if (++hops > kMaxPointerHops) {
        return false;
      }
      const size_t target = (static_cast<size_t>(c & 0x3F) << 8) | static_cast<uint8_t>(buf[p + 1]);
      if (target >= buf.size()) {
        return false;
      }
      p = target;
      continue;
    }
    if ((c & 0xC0) != 0) {
      return false;
    }
    if (c == 0) {
      if (!jumped) {
        *in_place = p + 1 - pos;
      }
      return true;
    }
    if (p + 1 + c > buf.size()) {
      return false;
    }
    decoded_len += c + 1;
    if (decoded_len > kMaxNameLength) {
      return false;
    }
    if (out != nullptr) {
      if (!out->empty()) {
        out->push_back('.');
      }
      out->append(buf.substr(p + 1, c));
    }
    p += 1 + c;
  }
}

}  // namespace

bool WalkDNSMessage(std::string_view buf, DNSMessageInfo* info) {
  *info = DNSMessageInfo{};
  if (buf.size() < kHeaderSize) {
    return false;
  }
  info->txid = ReadU16(buf, 0);
  info->flags = ReadU16(buf, 2);
  const uint16_t num_questions = ReadU16(buf, 4);
  const uint32_t num_records =
      static_cast<uint32_t>(ReadU16(buf, 6)) + ReadU16(buf, 8) + ReadU16(buf, 10);
  const uint16_t num_answers = ReadU16(buf, 6);

  size_t pos = kHeaderSize;
  for (uint16_t i = 0; i < num_questions; ++i) {
    DNSQuestion q;
    size_t in_place = 0;
    if (!ReadName(buf, pos, &in_place, &q.name)) {
      return false;
    }
    pos += in_place;
    if (pos + kQuestionFixedSize > buf.size()) {
      return false;
    }
    q.qtype = ReadU16(buf, pos);
    pos += kQuestionFixedSize;
    info->questions.push_back(std::move(q));
  }

  for (uint32_t i = 0; i < num_records; ++i) {
    const size_t owner_pos = pos;
    size_t in_place = 0;
    if (!ReadName(buf, pos, &in_place, nullptr)) {
      return false;
    }
    pos += in_place;
    if (pos + kRecordFixedSize > buf.size()) {
      return false;
    }
    const uint16_t type = ReadU16(buf, pos);
    const uint16_t rdlength = ReadU16(buf, pos + 8);
    const size_t rdata_pos = pos + kRecordFixedSize;
    if (rdata_pos + rdlength > buf.size()) {
      return false;
    }
    if (i < num_answers) {
      DNSAnswer a;
      a.type = type;
      size_t unused = 0;
      bool ok = ReadName(buf, owner_pos, &unused, &a.name);
      switch (type) {
        case kDNSTypeA:
        case kDNSTypeAAAA:
          ok = ok && rdlength == (type == kDNSTypeA ? 4 : 16);
          if (ok) {
            a.addr = std::string(buf.substr(rdata_pos, rdlength));
          }
          break;
        case kDNSTypeCNAME:
        case kDNSTypePTR:
          ok = ok && ReadName(buf, rdata_pos, &unused, &a.target);
          break;
        default:
          ok = false;
      }
      if (ok) {
        info->answers.push_back(std::move(a));
      }
    }
    pos = rdata_pos + rdlength;
  }

  info->length = pos;
  return true;
}

std::string DNSTypeName(uint16_t type) {
  switch (type) {
    case 1:
      return "A";
    case 2:
      return "NS";
    case 5:
      return "CNAME";
    case 6:
      return "SOA";
    case 12:
      return "PTR";
    case 15:
      return "MX";
    case 16:
      return "TXT";
    case 28:
      return "AAAA";
    case 33:
      return "SRV";
    case 35:
      return "NAPTR";
    case 64:
      return "SVCB";
    case 65:
      return "HTTPS";
    case 255:
      return "ANY";
    default:
      return "TYPE" + std::to_string(type);
  }
}

}  // namespace dns
}  // namespace protocols
}  // namespace stirling
}  // namespace px
