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

#include <utility>
#include <vector>
#include <string>
#include "src/common/json/json.h"
#include "src/stirling/source_connectors/tcp_stats/bcc_bpf_intf/tcp_stats.h"

namespace px {
namespace stirling {
namespace json_output {

static inline const std::string_view unspec = "UNSPEC";
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

static void AddRecHeaders(rapidjson::Document::AllocatorType& a,
                   rapidjson::Value &metricsRec ,
                   std::pair < ip_key_t, uint64_t > item,
                   const std::string_view name) {
  std::string addr_string;
  metricsRec.AddMember("name", json_output::StringRef(name), a);
  metricsRec.AddMember("event_type", json_output::StringRef(metrics_source), a);
  metricsRec.AddMember("type", "guage", a);
  metricsRec.AddMember("value", uint64_t(item.second), a);
  rapidjson::Value attributes(rapidjson::kObjectType);

  int family = item.first.addr.sa.sa_family;
  if (family == AF_INET) {
    addr_string = IPv4AddrToString(item.first.addr.in4.sin_addr).ConsumeValueOrDie();
    attributes.AddMember("remote-port",  item.first.addr.in4.sin_port, a);
  } else if (family == AF_INET6) {
    addr_string = IPv6AddrToString(item.first.addr.in6.sin6_addr).ConsumeValueOrDie();
    attributes.AddMember("remote-port", item.first.addr.in6.sin6_port, a);
  } else {
    addr_string = unspec;
  }
  attributes.AddMember("remote-ip",  std::string(addr_string), a);
  attributes.AddMember("process", std::string(item.first.name), a);
  metricsRec.AddMember("attributes", attributes.Move(), a);
  attributes.SetObject();
}

static void CreateRecords(rapidjson::Document::AllocatorType& a,
                   rapidjson::Value &metricsArray,
                   std::vector < std::pair < ip_key_t, uint64_t >> items,
                   const std::string_view name) {
  for (auto& item : items) {
    rapidjson::Value metricsRec(rapidjson::kObjectType);
    AddRecHeaders(a, metricsRec, item, name);
    metricsArray.PushBack(metricsRec.Move(), a);
    metricsRec.SetObject();
  }
}

}  // namespace json_output
}  // namespace stirling
}  // namespace px
