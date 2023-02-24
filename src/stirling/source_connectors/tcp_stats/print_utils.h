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

// This file contains helper functions and variables that are required
// to export data in New Relic compatable JSON format

#pragma once

#include <string>
#include <utility>
#include <vector>
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"

namespace px {
namespace stirling {
namespace json_output {

static inline const std::string_view unknown = "unknown";
static inline const std::string_view tx_metric = "eBPF.tcp_out_bound_throughput.metric";
static inline const std::string_view rx_metric = "eBPF.tcp_in_bound_throughput.metric";
static inline const std::string_view retrans_metric = "eBPF.tcp_retransmissions.metric";
static inline const std::string_view metrics_source = "tcp_stats";
static inline const std::string_view data_str = "data";
static inline const std::string_view metrics_str = "metrics";
static inline const std::string_view version_str = "protocol_version";
static inline const std::string_view version_value = "4";

inline rapidjson::GenericStringRef<char> StringRef(std::string_view s) {
  return rapidjson::GenericStringRef<char>(s.data(), s.size());
}

static std::string_view get_event(enum tcp_event_type_t t) {
  switch (t) {
    case kUnknownEvent:
      return unknown;
    case kTcpTx:
      return tx_metric;
    case kTcpRx:
      return rx_metric;
    case kTcpRetransmissions:
      return retrans_metric;
    default:
      return unknown;
  }
}

static void AddPerfRecHeaders(rapidjson::Document::AllocatorType& a, rapidjson::Value& metricsRec,
                              tcp_event_t item) {
  std::string_view addr_string;
  metricsRec.AddMember("name", json_output::StringRef(get_event(item.type)), a);
  metricsRec.AddMember("event_type", json_output::StringRef(metrics_source), a);
  metricsRec.AddMember("type", "gauge", a);
  metricsRec.AddMember("value", uint64_t(item.size), a);
  rapidjson::Value attributes(rapidjson::kObjectType);

  int family = item.addr.sa.sa_family;
  if (family == AF_INET) {
    addr_string = IPv4AddrToString(item.addr.in4.sin_addr).ConsumeValueOrDie();
    attributes.AddMember("remote-port", item.addr.in4.sin_port, a);
  } else if (family == AF_INET6) {
    addr_string = IPv6AddrToString(item.addr.in6.sin6_addr).ConsumeValueOrDie();
    attributes.AddMember("remote-port", item.addr.in6.sin6_port, a);
  } else {
    addr_string = unknown;
  }

  rapidjson::Value addr(rapidjson::kStringType);
  addr.SetString(addr_string.data(), addr_string.size(), a);
  attributes.AddMember("remote-ip", addr, a);

  std::string process = item.name;
  rapidjson::Value pname(rapidjson::kStringType);
  pname.SetString(process.data(), process.size(), a);
  attributes.AddMember("process", pname, a);
  metricsRec.AddMember("attributes", attributes.Move(), a);
  attributes.SetObject();
}

static void CreatePerfRecords(rapidjson::Document::AllocatorType& a, rapidjson::Value& metricsArray,
                              std::vector<tcp_event_t> items) {
  for (auto& item : items) {
    rapidjson::Value metricsRec(rapidjson::kObjectType);
    AddPerfRecHeaders(a, metricsRec, item);
    metricsArray.PushBack(metricsRec.Move(), a);
    metricsRec.SetObject();
  }
}

}  // namespace json_output
}  // namespace stirling
}  // namespace px
