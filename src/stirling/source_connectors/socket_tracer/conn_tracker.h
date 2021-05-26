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

#pragma once

#include <any>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <magic_enum.hpp>

#include "src/common/system/proc_parser.h"
#include "src/common/system/socket_info.h"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/go_grpc_types.hpp"
#include "src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.hpp"
#include "src/stirling/source_connectors/socket_tracer/data_stream.h"
#include "src/stirling/source_connectors/socket_tracer/fd_resolver.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/common/interface.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/http2_streams_container.h"
#include "src/stirling/source_connectors/socket_tracer/socket_trace_bpf_tables.h"
// Include all specializations of the StitchFrames() template specializations for all protocols.
#include "src/stirling/source_connectors/socket_tracer/protocols/stitchers.h"
#include "src/stirling/utils/stat_counter.h"

DECLARE_bool(treat_loopback_as_in_cluster);
DECLARE_int64(stirling_conn_trace_pid);
DECLARE_int64(stirling_conn_trace_fd);
DECLARE_bool(stirling_conn_disable_to_bpf);
DECLARE_int64(stirling_check_proc_for_conn_close);

#define CONN_TRACE(level) LOG_IF(INFO, level <= debug_trace_level_) << ToString() << " "

namespace px {
namespace stirling {

// Forward declaration to avoid circular include and conn_tracker.h.
class ConnTrackersManager;

/**
 * Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/source_connectors/socket_tracer/bcc_bpf_intf/socket_trace.h.
 */
struct SocketOpen {
  uint64_t timestamp_ns = 0;
  // TODO(yzhao): Consider using std::optional to indicate the address has not been initialized.
  SockAddr remote_addr;
};

struct SocketClose {
  uint64_t timestamp_ns = 0;
  // The number of bytes sent on the connection at time of close.
  uint64_t send_bytes = 0;
  // The number of bytes received on the connection at time of close.
  uint64_t recv_bytes = 0;
};

/**
 * Connection tracker is the main class that tracks all the events for a monitored TCP connection.
 *
 * It collects the connection info (e.g. remote IP, port),
 * and all the send/recv data observed on the connection.
 *
 * Data is extracted from a connection tracker and pushed out, as the data becomes parseable.
 */
class ConnTracker : NotCopyMoveable {
 public:
  enum class StatKey {
    // The number of sent/received data events.
    kDataEventSent,
    kDataEventRecv,

    // The number of sent/received bytes.
    kBytesSent,
    kBytesRecv,

    // The number of sent/received bytes that were transferred to user-space.
    kBytesSentTransferred,
    kBytesRecvTransferred,

    // The number of valid/invalid records.
    kValidRecords,
    kInvalidRecords,
  };

  // State values change monotonically from lower to higher values; and cannot change reversely.
  enum class State {
    // When collecting, the tracker collects data from BPF, but does not push them to table store.
    kCollecting,

    // When transferring, the tracker pushes data to table store.
    kTransferring,

    // When disabled, the tracker will silently drop existing data and silently drop any new data.
    // It will, however, still track open and close events.
    kDisabled,
  };

  class ConnStatsTracker {
   public:
    void set_closed(bool closed) { closed_ = closed; }

    void set_bytes_recv(int64_t bytes_recv) { bytes_recv_ = bytes_recv; }

    void set_bytes_sent(int64_t bytes_sent) { bytes_sent_ = bytes_sent; }

    int bytes_recv() { return bytes_recv_; }
    int bytes_sent() { return bytes_sent_; }
    int closed() { return closed_; }

    bool OpenSinceLastRead() {
      bool val = true - last_reported_open_;
      last_reported_open_ = true;
      return val;
    }
    bool CloseSinceLastRead() {
      bool val = closed_ - last_reported_close_;
      last_reported_close_ = closed_;
      return val;
    }
    int BytesRecvSinceLastRead() {
      int64_t val = bytes_recv_ - last_reported_bytes_recv_;
      last_reported_bytes_recv_ = bytes_recv_;
      return val;
    }
    int BytesSentSinceLastRead() {
      int64_t val = bytes_sent_ - last_reported_bytes_sent_;
      last_reported_bytes_sent_ = bytes_sent_;
      return val;
    }

   private:
    int64_t bytes_recv_ = 0;
    int64_t bytes_sent_ = 0;
    bool closed_ = false;

    int64_t last_reported_bytes_recv_ = 0;
    int64_t last_reported_bytes_sent_ = 0;
    bool last_reported_open_ = false;
    bool last_reported_close_ = false;
  };

  static constexpr std::chrono::seconds kDefaultInactivityDuration{300};

  /**
   * Number of TransferData() (i.e. PerfBuffer read) calls during which a ConnTracker
   * persists after it has been marked for death. We keep ConnTrackers alive to catch
   * late-arriving events, and for debug purposes.
   *
   * Note that an event may arrive appear to up to 1 iteration late.
   * This is caused by the order we read the perf buffers.   *
   * Example where event appears to arrive late:
   *  T0 - read perf buffer of data events
   *  T1 - DataEvent recorded
   *  T2 - CloseEvent recorded
   *  T3 - read perf buffer of close events <---- CloseEvent observed here
   *  ...
   *  T4 - read perf buffer of data events <---- DataEvent observed here
   * In such cases, the timestamps still show the DataEvent as occurring first.
   */
  static constexpr int64_t kDeathCountdownIters = 3;

  ConnTracker() = default;

  ~ConnTracker();

  /**
   * Registers a BPF connection control event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddControlEvent(const socket_control_event_t& event);

  /**
   * Registers a BPF data event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddDataEvent(std::unique_ptr<SocketDataEvent> event);

  /**
   * Registers a BPF connection stats event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddConnStats(const conn_stats_event_t& event);

  /**
   * Add a recorded HTTP2 header (name-value pair).
   * The struct should contain stream ID and other meta-data so it can matched with other HTTP2
   * header events and data frames.
   *
   * @param data The event from BPF uprobe.
   */
  void AddHTTP2Header(std::unique_ptr<HTTP2HeaderEvent> data);

  /**
   * Add a recorded HTTP2 data frame.
   * The struct should contain stream ID and other meta-data so it can matched with other HTTP2
   * header events and data frames.
   *
   * @param data The event from BPF uprobe.
   */
  void AddHTTP2Data(std::unique_ptr<HTTP2DataEvent> data);

  /**
   * Attempts to infer the remote endpoint of a connection.
   *
   * Intended for cases where the accept/connect was not traced.
   *
   * @param proc_parser Pointer to a proc_parser for access to /proc filesystem.
   * @param connections A map of inodes to endpoint information.
   */
  void InferConnInfo(system::ProcParser* proc_parser, system::SocketInfoManager* socket_info_mgr);

  /**
   * Processes the connection tracker, parsing raw events into frames,
   * and frames into record.
   *
   * @tparam TRecordType the type of the entries to be parsed.
   * @return Vector of processed entries.
   */
  template <typename TProtocolTraits>
  std::vector<typename TProtocolTraits::record_type> ProcessToRecords() {
    using TRecordType = typename TProtocolTraits::record_type;
    using TFrameType = typename TProtocolTraits::frame_type;
    using TStateType = typename TProtocolTraits::state_type;

    DataStreamsToFrames<TFrameType>();

    InitProtocolState<TStateType>();

    auto& req_frames = req_data()->Frames<TFrameType>();
    auto& resp_frames = resp_data()->Frames<TFrameType>();
    auto state_ptr = protocol_state<TStateType>();

    CONN_TRACE(1) << absl::Substitute("req_frames=$0 resp_frames=$1", req_frames.size(),
                                      resp_frames.size());

    protocols::RecordsWithErrorCount<TRecordType> result =
        protocols::StitchFrames<TRecordType, TFrameType, TStateType>(&req_frames, &resp_frames,
                                                                     state_ptr);

    CONN_TRACE(1) << absl::Substitute("records=$0", result.records.size());

    UpdateResultStats(result);

    return result.records;
  }

  /**
   * Returns reference to current set of unconsumed requests.
   * Note: A call to ProcessBytesToFrames() is required to parse new requests.
   */
  template <typename TFrameType>
  std::deque<TFrameType>& req_frames() {
    return req_data()->Frames<TFrameType>();
  }
  // TODO(yzhao): req_data() requires role_ to be set. But HTTP2 uprobe tracing does
  // not set that. So send_data() is created. Investigate more unified approach.
  template <typename TFrameType>
  const std::deque<TFrameType>& send_frames() const {
    return send_data_.Frames<TFrameType>();
  }

  size_t http2_client_streams_size() const { return http2_client_streams_.streams().size(); }
  size_t http2_server_streams_size() const { return http2_server_streams_.streams().size(); }

  /**
   * Returns reference to current set of unconsumed responses.
   * Note: A call to ProcessBytesToFrames() is required to parse new responses.
   */
  template <typename TFrameType>
  std::deque<TFrameType>& resp_frames() {
    return resp_data()->Frames<TFrameType>();
  }
  template <typename TFrameType>
  const std::deque<TFrameType>& recv_frames() const {
    return recv_data_.Frames<TFrameType>();
  }

  const conn_id_t& conn_id() const { return conn_id_; }
  TrafficProtocol protocol() const { return protocol_; }
  EndpointRole role() const { return role_; }
  ConnStatsTracker& conn_stats() { return conn_stats_; }

  /**
   * Get remote IP endpoint of the connection.
   *
   * @return IP.
   */
  const SockAddr& remote_endpoint() const { return open_info_.remote_addr; }

  /**
   * Get the connection information (e.g. remote IP, port, PID, etc.) for this connection.
   *
   * @return connection information.
   */
  const SocketOpen& conn() const { return open_info_; }

  /**
   * Get the DataStream of sent frames for this connection.
   *
   * @return Data stream of send data.
   */
  const DataStream& send_data() const { return send_data_; }

  /**
   * Get the DataStream of received frames for this connection.
   *
   * @return Data stream of received data.
   */
  const DataStream& recv_data() const { return recv_data_; }

  /**
   * Get the DataStream of requests for this connection.
   *
   * @return Data stream of requests.
   */
  DataStream* req_data();

  /**
   * Get the DataStream of responses for this connection.
   *
   * @return Data stream of responses.
   */
  DataStream* resp_data();

  /**
   * @return Returns the latest timestamp of all BPF events received by this tracker (using BPF
   * timestamp).
   */
  uint64_t last_bpf_timestamp_ns() { return last_bpf_timestamp_ns_; }

  /**
   * @return Returns the a timestamp the last time an event was added to this tracker (using
   * steady_clock).
   */
  std::chrono::time_point<std::chrono::steady_clock> last_update_timestamp() {
    return last_activity_timestamp_;
  }

  /**
   * Resets the state of the connection tracker, clearing all data and state.
   */
  void Reset();

  /**
   * Disables the connection tracker. The tracker will drop all its existing data,
   * and also not accept any future data (future data events will be ignored).
   *
   * The tracker will still wait for a Close event to get destroyed.
   */
  void Disable(std::string_view reason = "");

  /**
   * If disabled, returns the reason the tracker was disabled.
   */
  std::string_view disable_reason() const { return disable_reason_; }

  /**
   * Returns a state that determine the operations performed on the traffic traced on the
   * connection.
   */
  State state() const { return state_; }

  /**
   * Check if all events have been received on this stream.
   * Implies that the Close() event has been received as well.
   *
   * @return whether all data events and connection close have been received.
   */
  bool AllEventsReceived() const;

  /**
   * Marks the ConnTracker for death.
   *
   * This indicates that the tracker should not receive any further events,
   * otherwise an warning or error will be produced.
   */
  void MarkForDeath(int32_t countdown = kDeathCountdownIters);

  /**
   * Returns true if this tracker has been marked for death.
   *
   * @return true if this tracker is on its death countdown.
   */
  bool IsZombie() const;

  /**
   * Marks the ConnTracker as having reported its final conn stats event.
   */
  void MarkFinalConnStatsReported() { final_conn_stats_reported_ = true; }

  /**
   * Whether this ConnTracker can be destroyed.
   * @return true if this ConnTracker is a candidate for destruction.
   */
  bool ReadyForDestruction() const;

  void set_current_time(std::chrono::time_point<std::chrono::steady_clock> time) {
    DCHECK(time >= current_time_);
    current_time_ = time;
  }

  /**
   * Performs any preprocessing that should happen per iteration on this
   * connection tracker.
   * Should be called once per sampling, before ProcessToRecords().
   *
   * @param proc_parser Pointer to a proc_parser for access to /proc filesystem.
   * @param connections A map of inodes to endpoint information.
   */
  void IterationPreTick(const std::chrono::time_point<std::chrono::steady_clock>& iteration_time,
                        const std::vector<CIDRBlock>& cluster_cidrs,
                        system::ProcParser* proc_parser,
                        system::SocketInfoManager* socket_info_mgr);

  /**
   * Updates the any state that changes per iteration on this connection tracker.
   * Should be called once per sampling, after ProcessToRecords().
   */
  void IterationPostTick();

  /**
   * Sets the duration after which a connection is deemed to be inactive.
   * After becoming inactive, the connection may either (1) have its buffers purged,
   * where any unparsed frames are discarded or (2) be removed entirely from the
   * set of tracked connections. The main difference between (1) and (2) are that
   * in (1) some connection information is retained in case the connection becomes
   * active again.
   *
   * NOTE: This function is static, because it is currently only intended to be
   * used for testing purposes. If ever a need arises to have different inactivity
   * durations per connection tracker, then this function (and related functions below)
   * should be made into a member function.
   *
   * @param duration The duration in seconds, with no events, after which a connection
   * is deemed to be inactive.
   */
  static void set_inactivity_duration(std::chrono::seconds duration) {
    inactivity_duration_ = duration;
  }

  /**
   * Return the currently configured duration, after which a connection is deemed to be inactive.
   */
  static std::chrono::seconds InactivityDuration() { return inactivity_duration_; }

  /**
   * Fraction of frame stitching attempts that resulted in an invalid record.
   */
  double StitchFailureRate() const;

  uint64_t GetStat(StatKey key) const { return stats_.Get(key); }

  /**
   * Initializes protocol state for a protocol.
   */
  template <typename TStateType>
  void InitProtocolState() {
    // A protocol can specify that it has no state by setting ProtocolTraits::state_type to
    // NoState.
    // As an optimization, we don't call std::make_unique in such cases.
    // No need to create an object on the heap for protocols that don't have state.
    // Note that protocol_state() has the same `if constexpr`, for this optimization to work.
    if constexpr (!std::is_same_v<TStateType, protocols::NoState>) {
      TStateType* state_types_ptr = std::any_cast<TStateType>(&protocol_state_);
      if (state_types_ptr == nullptr) {
        protocol_state_.emplace<TStateType>();
      }
    }
  }

  /**
   * Returns the current protocol state for a protocol.
   */
  template <typename TStateType>
  TStateType* protocol_state() {
    // See note in InitProtocolState about this `if constexpr`.
    if constexpr (std::is_same_v<TStateType, protocols::NoState>) {
      return nullptr;
    } else {
      TStateType* ptr = std::any_cast<TStateType>(&protocol_state_);
      return ptr;
    }
  }

  template <typename TProtocolTraits>
  void Cleanup(size_t size_limit_bytes,
               std::chrono::time_point<std::chrono::steady_clock> expiry_timestamp) {
    using TFrameType = typename TProtocolTraits::frame_type;
    using TStateType = typename TProtocolTraits::state_type;

    if constexpr (std::is_same_v<TFrameType, protocols::http2::Stream>) {
      http2_client_streams_.Cleanup(size_limit_bytes, expiry_timestamp);
      http2_server_streams_.Cleanup(size_limit_bytes, expiry_timestamp);
    } else {
      send_data_.CleanupFrames<TFrameType>(size_limit_bytes, expiry_timestamp);
      recv_data_.CleanupFrames<TFrameType>(size_limit_bytes, expiry_timestamp);
    }

    auto* state = protocol_state<TStateType>();
    if (send_data_.CleanupEvents()) {
      if (state != nullptr) {
        state->global = {};
        state->send = {};
      }
    }
    if (recv_data_.CleanupEvents()) {
      if (state != nullptr) {
        state->global = {};
        state->recv = {};
      }
    }
  }

  static void SetConnInfoMapManager(const std::shared_ptr<ConnInfoMapManager>& conn_info_map_mgr) {
    conn_info_map_mgr_ = conn_info_map_mgr;
  }

  void SetConnID(struct conn_id_t conn_id);

  void SetRemoteAddr(const union sockaddr_t addr, std::string_view reason);

  // Returns false if the protocol change is disallowed.
  bool SetProtocol(TrafficProtocol protocol, std::string_view reason);

  // Returns true if the state was changed.
  bool SetRole(EndpointRole role, std::string_view reason);

  void SetDebugTrace(int level) { debug_trace_level_ = level; }

  std::string ToString() const;

 private:
  /**
   * The iterations given for protocol detection by uprobes. The value is given to the worst
   * situation when the uprobe events are polled after the kprobe events.
   *
   * In the first iteration, the uprobe events are not polled at all. Because the uprobe
   * events are not submitted to the perf buffer.
   *
   * In the 2nd iteration, the kprobe events are polled first, but the uprobe events are polled
   * later.
   *
   * Here we do not consider event loss.
   */
  static constexpr int64_t kUProbeProtocolDetectionIters = 2;

  // The duration after which a connection is deemed to be inactive.
  inline static std::chrono::seconds inactivity_duration_ = kDefaultInactivityDuration;

  // conn_info_map_mgr_ is used to release BPF map resources when a ConnTracker is destroyed.
  // It is a safety net, since BPF should release the resources as long as the close() syscall is
  // made. Note that since there is only one global BPF map, this is a static/global structure.
  inline static std::shared_ptr<ConnInfoMapManager> conn_info_map_mgr_;

  void AddConnOpenEvent(const conn_event_t& conn_info, uint64_t timestamp_ns);
  void AddConnCloseEvent(const close_event_t& close_event, uint64_t timestamp_ns);

  void UpdateTimestamps(uint64_t bpf_timestamp);

  // Called when any events were received for a connection.
  void CheckTracker();

  void CheckProcForConnClose();
  void HandleInactivity();
  bool IsRemoteAddrInCluster(const std::vector<CIDRBlock>& cluster_cidrs);
  void UpdateState(const std::vector<CIDRBlock>& cluster_cidrs);

  void UpdateDataStats(const SocketDataEvent& event);

  template <typename TFrameType>
  void DataStreamsToFrames() {
    DataStream* resp_data_ptr = resp_data();
    DCHECK_NE(resp_data_ptr, nullptr);
    resp_data_ptr->template ProcessBytesToFrames<TFrameType>(MessageType::kResponse);

    DataStream* req_data_ptr = req_data();
    DCHECK_NE(req_data_ptr, nullptr);
    req_data_ptr->template ProcessBytesToFrames<TFrameType>(MessageType::kRequest);
  }

  template <typename TRecordType>
  void UpdateResultStats(const protocols::RecordsWithErrorCount<TRecordType>& result) {
    stats_.Increment(StatKey::kInvalidRecords, result.error_count);
    stats_.Increment(StatKey::kValidRecords, result.records.size());
  }

  int debug_trace_level_ = 0;

  // Used to identify the remove endpoint in case the accept/connect was not traced.
  std::unique_ptr<FDResolver> conn_resolver_ = nullptr;
  bool conn_resolution_failed_ = false;

  struct conn_id_t conn_id_ = {};

  TrafficProtocol protocol_ = kProtocolUnknown;
  EndpointRole role_ = kRoleUnknown;
  SocketOpen open_info_;
  SocketClose close_info_;
  ConnStatsTracker conn_stats_;
  uint64_t last_conn_stats_update_ = 0;
  bool final_conn_stats_reported_ = false;

  // The data collected by the stream, one per direction.
  DataStream send_data_;
  DataStream recv_data_;

  // Uprobe-based HTTP2 uses a different scheme, where it holds client and server-initiated streams,
  // instead of send and recv messages. As such, we create aliases for HTTP2.
  HTTP2StreamsContainer http2_client_streams_;
  HTTP2StreamsContainer http2_server_streams_;

  // Access the appropriate HalfStream object for the given stream ID.
  protocols::http2::HalfStream* HalfStreamPtr(uint32_t stream_id, bool write_event);

  // The timestamp of the last activity on this connection.
  // Recorded as the latest timestamp on a BPF event.
  uint64_t last_bpf_timestamp_ns_ = 0;

  std::chrono::time_point<std::chrono::steady_clock> current_time_;

  // The timestamp of the last update on this connection which alters the states.
  // Recorded as the latest activity time on the ConnTracker.
  // Currently using steady clock, so cannot be used meaningfully for logging real times.
  std::chrono::time_point<std::chrono::steady_clock> last_activity_timestamp_;

  // Filter for less spammy trace logs.
  bool suppress_fd_link_log_ = false;

  // Some idleness checks used to trigger checks for closed connections.
  // The threshold undergoes an exponential backoff if connection is not closed.
  bool idle_iteration_ = false;
  int idle_iteration_count_ = 0;
  int idle_iteration_threshold_ = 2;

  State state_ = State::kCollecting;

  std::string disable_reason_;

  // Iterations before the tracker can be killed.
  int32_t death_countdown_ = -1;

  utils::StatCounter<StatKey> stats_;

  // Connection trackers need to keep a state because there can be information between
  // needed from previous requests/responses needed to parse or render current request.
  // E.g. MySQL keeps a map of previously occurred stmt prepare events as the state such
  // that future stmt execute events can match up with the correct one using stmt_id.
  //
  // TODO(oazizi): Is there a better structure than std::any?
  // One alternative is an std::variant, but that becomes tedious to maintain with a
  // growing number of protocols.
  // Two considerations:
  // 1) We want something with an interface-type API. The current structure does achieve
  //    this, but not in a clear way. The compilation errors will be due to SFINAE and
  //    hard to interpret.
  // 2) We want something with some type safety. std::any does provide this, in the
  //    similar way as std::variant.
  std::any protocol_state_;

  template <typename TProtocolTraits>
  friend std::string DebugString(const ConnTracker& c, std::string_view prefix);

  // A pointer to the conn trackers manager, used for notifying a protocol change.
  ConnTrackersManager* manager_ = nullptr;

  friend class ConnTrackersManager;
  friend class ConnTrackersManagerTest;
  friend class ConnTrackerGenerationsTest;
};

// Explicit template specialization must be declared in namespace scope.
// See https://en.cppreference.com/w/cpp/language/member_template
template <>
std::vector<protocols::http2::Record>
ConnTracker::ProcessToRecords<protocols::http2::ProtocolTraits>();

template <typename TProtocolTraits>
std::string DebugString(const ConnTracker& c, std::string_view prefix) {
  using TFrameType = typename TProtocolTraits::frame_type;

  std::string info;
  info += absl::Substitute("$0conn_id=$1\n", prefix, ToString(c.conn_id()));
  info += absl::Substitute("state=$0\n", magic_enum::enum_name(c.state()));
  info += absl::Substitute("$0remote_addr=$1:$2\n", prefix, c.remote_endpoint().AddrStr(),
                           c.remote_endpoint().port());
  info += absl::Substitute("$0protocol=$1\n", prefix, magic_enum::enum_name(c.protocol()));
  info += absl::Substitute("$0recv queue\n", prefix);
  info += absl::Substitute("$0send queue\n", prefix);
  if constexpr (std::is_same_v<TFrameType, protocols::http2::Stream>) {
    info += c.http2_client_streams_.DebugString(absl::StrCat(prefix, "  "));
    info += c.http2_server_streams_.DebugString(absl::StrCat(prefix, "  "));
  } else {
    info += DebugString<TFrameType>(c.recv_data(), absl::StrCat(prefix, "  "));
    info += DebugString<TFrameType>(c.send_data(), absl::StrCat(prefix, "  "));
  }

  return info;
}

}  // namespace stirling
}  // namespace px
