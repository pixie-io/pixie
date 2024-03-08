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
#include "src/common/base/inet_utils.h"
#include "src/common/system/kernel_version.h"
#include "src/stirling/bpf_tools/macros.h"
#include "src/stirling/source_connectors/tcp_stats/tcp_stats.h"

OBJ_STRVIEW(tcpstats_bcc_script, tcpstats);

namespace px {
namespace stirling {

// Allocating 50 MB perf buffer. It can accommodate ~125000 events
// Considering each event size ~400 bytes (struct tcp_event_t).
constexpr uint32_t kPerfBufferPerCPUSizeBytes = 50 * 1024 * 1024;

using ProbeType = bpf_tools::BPFProbeAttachType;

const auto kProbeSpecs = MakeArray<bpf_tools::KProbeSpec>(
    {{"tcp_sendmsg", ProbeType::kEntry, "probe_entry_tcp_sendmsg", /*is_syscall*/ false},
     {"tcp_sendmsg", ProbeType::kReturn, "probe_ret_tcp_sendmsg", /*is_syscall*/ false},
     {"tcp_cleanup_rbuf", ProbeType::kEntry, "probe_entry_tcp_cleanup_rbuf", /*is_syscall*/ false},
     {"tcp_retransmit_skb", ProbeType::kEntry, "probe_entry_tcp_retransmit_skb",
      /*is_syscall*/ false}});

const auto kSendPageProbeSpecs = MakeArray<bpf_tools::KProbeSpec>(
    {{"tcp_sendpage", ProbeType::kEntry, "probe_entry_tcp_sendpage", /*is_syscall*/ false},
     {"tcp_sendpage", ProbeType::kReturn, "probe_ret_tcp_sendpage", /*is_syscall*/ false}});

void HandleTcpEvent(void* cb_cookie, void* data, int /*data_size*/) {
  auto* connector = reinterpret_cast<TCPStatsConnector*>(cb_cookie);
  auto* event = reinterpret_cast<struct tcp_event_t*>(data);
  connector->AcceptTcpEvent(*event);
}

void TCPStatsConnector::AcceptTcpEvent(const struct tcp_event_t& event) {
  events_.push_back(event);
}

void HandleTcpEventLoss(void* /*cb_cookie*/, uint64_t /*lost*/) {
  // TODO(RagalahariP): Add stats counter.
}

Status TCPStatsConnector::InitImpl() {
  const auto perf_buffer_specs = MakeArray<bpf_tools::PerfBufferSpec>({
      {"tcp_events", HandleTcpEvent, HandleTcpEventLoss, this, kPerfBufferPerCPUSizeBytes,
       bpf_tools::PerfBufferSizeCategory::kData},
  });

  sampling_freq_mgr_.set_period(kSamplingPeriod);
  push_freq_mgr_.set_period(kPushPeriod);
  PX_RETURN_IF_ERROR(bcc_->InitBPFProgram(tcpstats_bcc_script));
  PX_RETURN_IF_ERROR(bcc_->AttachKProbes(kProbeSpecs));
  const auto sendpage_attach_status = bcc_->AttachKProbes(kSendPageProbeSpecs);
  if (!sendpage_attach_status.ok()) {
    const auto kernel_version = system::GetCachedKernelVersion();
    LOG(INFO) << absl::Substitute(
        "Could not attach tcp_sendpage probes: $0, detected kernel version: $1. Note: tcp_sendpage "
        "was removed in Kernel 6.5.",
        sendpage_attach_status.msg(), kernel_version.ToString());
  }
  PX_RETURN_IF_ERROR(bcc_->OpenPerfBuffers(perf_buffer_specs));
  LOG(INFO) << absl::Substitute("Successfully deployed $0 kprobes.", kProbeSpecs.size());
  return Status::OK();
}

Status TCPStatsConnector::StopImpl() {
  bcc_->Close();
  return Status::OK();
}

void TCPStatsConnector::TransferDataImpl(ConnectorContext* ctx) {
  DCHECK_EQ(data_tables_.size(), 1U) << "Only one table is allowed per TCPStatsConnector.";

  bcc_->PollPerfBuffers();

  DataTable* data_table = data_tables_[0];
  auto* agg_stats = tcp_stats_.UpdateStats(events_);
  uint64_t time = AdjustedSteadyClockNowNS();
  absl::flat_hash_set<md::UPID> upids = ctx->GetUPIDs();

  auto iter = agg_stats->begin();
  while (iter != agg_stats->end()) {
    const auto& key = iter->first;
    auto& stats = iter->second;

    md::UPID upid(ctx->GetASID(), key.upid.pid, key.upid.start_time_ticks);

    DataTable::RecordBuilder<&tcp_stats::kTCPStatsTable> r(data_table, time);
    r.Append<tcp_stats::kTCPTimeIdx>(time);
    r.Append<tcp_stats::kTCPUPIDIdx>(upid.value());
    r.Append<tcp_stats::kTCPLocalAddrIdx>(key.local_addr);
    r.Append<tcp_stats::kTCPLocalPortIdx>(key.local_port);
    r.Append<tcp_stats::kTCPRemoteAddrIdx>(key.remote_addr);
    r.Append<tcp_stats::kTCPRemotePortIdx>(key.remote_port);
    r.Append<tcp_stats::kTCPBytesReceivedIdx>(stats.bytes_recv);
    r.Append<tcp_stats::kTCPBytesSentIdx>(stats.bytes_sent);
    r.Append<tcp_stats::kTCPRetransmitsIdx>(stats.retransmissions);

    agg_stats->erase(iter++);
  }

  events_.clear();
}
}  // namespace stirling
}  // namespace px
