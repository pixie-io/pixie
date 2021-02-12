#pragma once

#include "src/stirling/core/types.h"
#include "src/stirling/source_connectors/socket_tracer/canonical_types.h"

namespace pl {
namespace stirling {

// clang-format off
constexpr DataElement kConnStatsElements[] = {
        canonical_data_elements::kTime,
        canonical_data_elements::kUPID,
        canonical_data_elements::kRemoteAddr,
        canonical_data_elements::kRemotePort,
        {"addr_family", "The socket address family of the connection.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM,
         &kSockAddrFamilyDecoder},
        {"protocol", "The protocol of the traffic on the connections.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM,
         &kTrafficProtocolDecoder},
        // TODO(yzhao): Might want to replace this with "trace_role" for consistency.
        {"role", "The role of the process that owns the connections.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::GENERAL_ENUM,
         &kEndpointRoleDecoder},
        {"conn_open", "The number of connections opened since the beginning of tracing.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"conn_close", "The number of connections closed since the beginning of tracing.",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_COUNTER},
        {"conn_active", "The number of active connections",
         types::DataType::INT64, types::SemanticType::ST_NONE, types::PatternType::METRIC_GAUGE},
        {"bytes_sent", "The number of bytes sent to the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
        {"bytes_recv", "The number of bytes received from the remote endpoint(s).",
         types::DataType::INT64, types::SemanticType::ST_BYTES, types::PatternType::METRIC_COUNTER},
#ifndef NDEBUG
        {"px_info_", "Pixie messages regarding the record (e.g. warnings)",
         types::DataType::STRING, types::SemanticType::ST_NONE, types::PatternType::GENERAL},
#endif
};
// clang-format on

constexpr DataTableSchema kConnStatsTable(
    "conn_stats",
    "Connection-level stats. This table contains statistics on the communications made between "
    "client-server pairs. For network-level information such as RX/TX errors and drops, see the "
    "Network-Layer Stats (network_stats) table.",
    kConnStatsElements,
    /* default_sampling_period */ std::chrono::milliseconds{5000},
    /* default_push_period */ std::chrono::milliseconds{5000});

namespace conn_stats_idx {

constexpr int kTime = kConnStatsTable.ColIndex("time_");
constexpr int kUPID = kConnStatsTable.ColIndex("upid");
constexpr int kRemoteAddr = kConnStatsTable.ColIndex("remote_addr");
constexpr int kRemotePort = kConnStatsTable.ColIndex("remote_port");
constexpr int kAddrFamily = kConnStatsTable.ColIndex("addr_family");
constexpr int kProtocol = kConnStatsTable.ColIndex("protocol");
constexpr int kRole = kConnStatsTable.ColIndex("role");
constexpr int kConnOpen = kConnStatsTable.ColIndex("conn_open");
constexpr int kConnClose = kConnStatsTable.ColIndex("conn_close");
constexpr int kConnActive = kConnStatsTable.ColIndex("conn_active");
constexpr int kBytesSent = kConnStatsTable.ColIndex("bytes_sent");
constexpr int kBytesRecv = kConnStatsTable.ColIndex("bytes_recv");
#ifndef NDEBUG
constexpr int kPxInfo = kConnStatsTable.ColIndex("px_info_");
#endif

}  // namespace conn_stats_idx

}  // namespace stirling
}  // namespace pl
