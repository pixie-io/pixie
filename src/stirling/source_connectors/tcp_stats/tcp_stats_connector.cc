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

#include "src/stirling/source_connectors/tcp_stats/tcp_stats_connector.h"

#include <arpa/inet.h>
#include <string>
#include <utility>
#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/tcp_stats/print_utils.h"

BPF_SRC_STRVIEW(tcpstats_bcc_script, tcpstats);
DEFINE_bool(JsonOutput, true, "Print on santdard in Json format");

namespace px {
namespace stirling {

using ProbeType = bpf_tools::BPFProbeAttachType;
const auto kProbeSpecs = MakeArray<bpf_tools::KProbeSpec>(
  {{"tcp_sendmsg", ProbeType::kEntry, "probe_entry_tcp_sendmsg", /*is_syscall*/ false},
   {"tcp_sendmsg", ProbeType::kReturn, "probe_ret_tcp_sendmsg", /*is_syscall*/ false},
   {"tcp_cleanup_rbuf", ProbeType::kEntry, "probe_entry_tcp_cleanup_rbuf", /*is_syscall*/ false},
   {"tcp_retransmit_skb", ProbeType::kEntry, "probe_entry_tcp_retransmit_skb", /*is_syscall*/ false}});


Status TCPStatsConnector::InitImpl() {
  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  PL_RETURN_IF_ERROR(InitBPFProgram(tcpstats_bcc_script));
  PL_RETURN_IF_ERROR(AttachKProbes(kProbeSpecs));
  LOG(INFO) << absl::Substitute("Number of kprobes deployed = $0", kProbeSpecs.size());
  LOG(INFO) << "Probes successfully deployed.";
  return Status::OK();
}

Status TCPStatsConnector::StopImpl() {
  Close();
  return Status::OK();
}

void TCPStatsConnector::TransferDataImpl(ConnectorContext* /* ctx */) {
  if ( FLAGS_JsonOutput != true ) {
    /* TODO: Support other output formats */
    return;
  }

  rapidjson::Document document;
  document.SetObject();
  rapidjson::Document::AllocatorType & allocator = document.GetAllocator();
  rapidjson::Value metricsArray(rapidjson::kArrayType);

  constexpr bool kClearTable = true;

  if (data_tables_[kTCPTXStatsTableNum] != nullptr) {
    std::vector < std::pair < ip_key_t, uint64_t >> tx_items =
     GetHashTable<ip_key_t, uint64_t>("sent_bytes").get_table_offline(kClearTable);
    json_output::CreateRecords(allocator, metricsArray, tx_items, json_output::tx_metric);
  }

  if (data_tables_[kTCPTXStatsTableNum] != nullptr) {
     std::vector < std::pair < ip_key_t, uint64_t >> rx_items =
       GetHashTable < ip_key_t, uint64_t > ("recv_bytes").get_table_offline(kClearTable);
     json_output::CreateRecords(allocator, metricsArray, rx_items, json_output::rx_metric);
  }

  if (data_tables_[kTCPRetransStatsTableNum] != nullptr) {
     std::vector < std::pair < ip_key_t, uint64_t >> retrans_items =
       GetHashTable < ip_key_t, uint64_t > ("retrans").get_table_offline(kClearTable);
     json_output::CreateRecords(allocator, metricsArray, retrans_items, json_output::retrans_metric);
  }

  rapidjson::Value data(rapidjson::kObjectType);
  data.AddMember(json_output::StringRef(json_output::data_str), metricsArray.Move(), allocator);
  document.AddMember(json_output::StringRef(json_output::version_str), json_output::StringRef(json_output::version_value), allocator);
  rapidjson::Value dataArray(rapidjson::kArrayType);
  dataArray.PushBack(data.Move(), allocator);
  document.AddMember(json_output::StringRef(json_output::data_str), dataArray.Move(), allocator);

  rapidjson::StringBuffer strbuf;
  rapidjson::Writer < rapidjson::StringBuffer > writer(strbuf);
  document.Accept(writer);
  std::cout << strbuf.GetString() << std::endl;
}
}  // namespace stirling
}  // namespace px
