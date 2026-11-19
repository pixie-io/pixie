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

#include "src/stirling/source_connectors/socket_tracer/protocols/websocket/k8s_exec.h"

#include <algorithm>
#include <utility>

#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {
namespace protocols {
namespace websocket {

namespace {

constexpr std::string_view kChannelProtocolSuffix = "channel.k8s.io";
constexpr std::string_view kBase64ChannelProtocol = "base64.channel.k8s.io";
constexpr uint8_t kStdinChannel = 0;
constexpr uint8_t kStdoutChannel = 1;
constexpr uint8_t kStderrChannel = 2;
constexpr uint8_t kStatusChannel = 3;
constexpr size_t kMaxBufferedOutput = 4096;
constexpr char kEsc = '\x1b';
constexpr char kBel = '\x07';
constexpr char kBackspace = '\x08';
constexpr char kDelete = '\x7f';
constexpr char kCtrlC = '\x03';
constexpr char kCtrlD = '\x04';
constexpr char kCtrlU = '\x15';

int HexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string QueryUnescape(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      out.push_back(' ');
    } else if (value[i] == '%' && i + 2 < value.size() && HexValue(value[i + 1]) >= 0 &&
               HexValue(value[i + 2]) >= 0) {
      out.push_back(static_cast<char>(HexValue(value[i + 1]) * 16 + HexValue(value[i + 2])));
      i += 2;
    } else {
      out.push_back(value[i]);
    }
  }
  return out;
}

std::vector<std::string> QueryValues(std::string_view path, std::string_view key) {
  std::vector<std::string> values;
  size_t query_start = path.find('?');
  if (query_start == std::string_view::npos) {
    return values;
  }
  for (std::string_view param : absl::StrSplit(path.substr(query_start + 1), '&')) {
    std::pair<std::string_view, std::string_view> kv =
        absl::StrSplit(param, absl::MaxSplits('=', 1));
    if (kv.first == key) {
      values.push_back(QueryUnescape(kv.second));
    }
  }
  return values;
}

void PopUTF8Char(std::string* line) {
  while (!line->empty()) {
    const bool continuation = (static_cast<uint8_t>(line->back()) & 0xc0) == 0x80;
    line->pop_back();
    if (!continuation) {
      return;
    }
  }
}

std::string StripEscapeSequences(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != kEsc) {
      out.push_back(text[i]);
      continue;
    }
    if (i + 1 >= text.size()) {
      break;
    }
    if (text[i + 1] == '[') {
      i += 2;
      while (i < text.size() && (text[i] < 0x40 || text[i] > 0x7e)) {
        ++i;
      }
    } else if (text[i + 1] == ']') {
      i += 2;
      while (i < text.size() && text[i] != kBel &&
             !(text[i] == kEsc && i + 1 < text.size() && text[i + 1] == '\\')) {
        ++i;
      }
      if (i < text.size() && text[i] == kEsc) {
        ++i;
      }
    } else {
      ++i;
    }
  }
  return out;
}

Record MakeRecord(RecordKind kind, message_type_t type, uint64_t timestamp_ns,
                  std::string payload) {
  Record record;
  record.kind = kind;
  record.msg.timestamp_ns = timestamp_ns;
  record.msg.type = type;
  record.msg.opcode = Opcode::kBinary;
  record.msg.fragments = 1;
  record.msg.payload = std::move(payload);
  return record;
}

void TakeOutput(ExecState* state, Record* record) {
  record->output = CleanTerminalOutput(state->output, state->tty);
  record->output_size = state->output_size;
  state->output.clear();
  state->output_size = 0;
}

void AppendOutput(ExecState* state, std::string_view data) {
  state->output_size += data.size();
  const size_t room = kMaxBufferedOutput - std::min(kMaxBufferedOutput, state->output.size());
  state->output.append(data.substr(0, room));
}

void EmitPending(ExecState* state, std::vector<Record>* records) {
  if (!state->pending.has_value()) {
    return;
  }
  Record record = MakeRecord(RecordKind::kInput, message_type_t::kRequest,
                             state->pending->timestamp_ns, std::move(state->pending->line));
  TakeOutput(state, &record);
  records->push_back(std::move(record));
  state->pending.reset();
}

void SubmitLine(ExecState* state, std::string_view suffix, std::vector<Record>* records) {
  EmitPending(state, records);
  state->pending = PendingInput{state->line_timestamp_ns, absl::StrCat(state->line, suffix)};
  state->line.clear();
  state->output.clear();
  state->output_size = 0;
}

void HandleStdin(uint64_t timestamp_ns, std::string_view data, ExecState* state,
                 std::vector<Record>* records) {
  for (char c : data) {
    const bool after_cr = state->last_cr;
    state->last_cr = c == '\r';
    switch (c) {
      case '\r':
      case '\n':
        if (!state->line.empty()) {
          SubmitLine(state, "", records);
        } else if (c == '\n' && after_cr) {
          continue;
        }
        break;
      case kBackspace:
      case kDelete:
        PopUTF8Char(&state->line);
        break;
      case kCtrlU:
        state->line.clear();
        break;
      case kCtrlC:
        if (!state->line.empty()) {
          SubmitLine(state, "^C", records);
        }
        break;
      case kCtrlD:
        if (state->line.empty()) {
          state->line_timestamp_ns = timestamp_ns;
          SubmitLine(state, "^D", records);
        }
        break;
      default:
        if (state->line.empty()) {
          state->line_timestamp_ns = timestamp_ns;
        }
        state->line.push_back(c);
        break;
    }
  }
}

}  // namespace

bool IsK8sChannelProtocol(std::string_view subprotocol) {
  return absl::EndsWith(subprotocol, kChannelProtocolSuffix) &&
         !absl::EndsWith(subprotocol, kBase64ChannelProtocol);
}

std::string K8sExecCommand(std::string_view path) {
  return absl::StrJoin(QueryValues(path, "command"), " ");
}

bool K8sExecTTY(std::string_view path) {
  for (const std::string& value : QueryValues(path, "tty")) {
    if (value == "true" || value == "1") {
      return true;
    }
  }
  return false;
}

std::string CleanTerminalOutput(std::string_view output, bool tty) {
  if (!tty) {
    return std::string(output);
  }
  std::string text = StripEscapeSequences(output);
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
      continue;
    }
    out.push_back(text[i]);
  }
  if (absl::StartsWith(out, "\n")) {
    out.erase(0, 1);
  }
  const size_t last_newline = out.rfind('\n');
  out.resize(last_newline == std::string::npos ? 0 : last_newline + 1);
  return out;
}

std::string Printable(std::string_view data) {
  const bool printable = std::all_of(data.begin(), data.end(), [](char c) {
    return (c >= 0x20 && c < 0x7f) || c == '\n' || c == '\t';
  });
  if (printable) {
    return std::string(data);
  }
  return BytesToString<bytes_format::HexAsciiMix>(data);
}

void DecodeK8sExec(std::string_view path, std::vector<Message> messages, ExecState* state,
                   std::vector<Record>* records) {
  for (Message& msg : messages) {
    if (IsControl(msg.opcode) || msg.payload.empty()) {
      continue;
    }
    if (!state->started) {
      state->started = true;
      state->tty = K8sExecTTY(path);
      records->push_back(MakeRecord(RecordKind::kExec, message_type_t::kRequest, msg.timestamp_ns,
                                    K8sExecCommand(path)));
    }
    const uint8_t channel = msg.payload[0];
    std::string_view data = std::string_view(msg.payload).substr(1);
    if (msg.type == message_type_t::kRequest) {
      if (channel == kStdinChannel) {
        HandleStdin(msg.timestamp_ns, data, state, records);
      }
      continue;
    }
    switch (channel) {
      case kStdoutChannel:
      case kStderrChannel:
        if (state->line.empty()) {
          AppendOutput(state, data);
        }
        break;
      case kStatusChannel: {
        EmitPending(state, records);
        Record record = MakeRecord(RecordKind::kExit, message_type_t::kResponse, msg.timestamp_ns,
                                   std::string(data));
        TakeOutput(state, &record);
        records->push_back(std::move(record));
        break;
      }
      default:
        break;
    }
  }
}

}  // namespace websocket
}  // namespace protocols
}  // namespace stirling
}  // namespace px
