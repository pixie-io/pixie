#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "src/common/system/proc_parser.h"
#include "src/common/system/socket_info.h"
#include "src/stirling/bcc_bpf_interface/go_grpc_types.h"
#include "src/stirling/common/go_grpc.h"
#include "src/stirling/common/socket_trace.h"
#include "src/stirling/data_stream.h"
#include "src/stirling/mysql/mysql_parse.h"
#include "src/stirling/socket_resolver.h"

DECLARE_bool(enable_unix_domain_sockets);
DECLARE_bool(infer_conn_info);

namespace pl {
namespace stirling {

/**
 * @brief Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf_interface/socket_trace.h.
 */
struct SocketOpen {
  uint64_t timestamp_ns = 0;
  IPAddress remote_addr;
};

struct SocketClose {
  uint64_t timestamp_ns = 0;
  // The send/write sequence number at time of close.
  uint64_t send_seq_num = 0;
  // The recv/read sequence number at time of close.
  uint64_t recv_seq_num = 0;
};

/**
 * Connection tracker is the main class that tracks all the events for a monitored TCP connection.
 *
 * It collects the connection info (e.g. remote IP, port),
 * and all the send/recv data observed on the connection.
 *
 * Data is extracted from a connection tracker and pushed out, as the data becomes parseable.
 */
class ConnectionTracker {
 public:
  // State values change monotonically from lower to higher values; and cannot change reversely.
  enum State {
    // When collecting, the tracker collects data from BPF, but does not push them to table store.
    kCollecting,

    // When transferring, the tracker pushes data to table store.
    kTransferring,

    // When disabled, the tracker will silently drop existing data and silently drop any new data.
    // It will, however, still track open and close events.
    kDisabled,
  };

  /**
   * @brief Registers a BPF connection control event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddControlEvent(const socket_control_event_t& event);

  /**
   * @brief Registers a BPF data event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddDataEvent(std::unique_ptr<SocketDataEvent> event);

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
   * @brief Attempts to infer the remote endpoint of a connection.
   *
   * Intended for cases where the accept/connect was not traced.
   *
   * @param proc_parser Pointer to a proc_parser for access to /proc filesystem.
   * @param connections A map of inodes to endpoint information.
   */
  void InferConnInfo(system::ProcParser* proc_parser,
                     const std::map<int, system::SocketInfo>* connections);

  /**
   * @brief Processes the connection tracker, parsing raw events into messages,
   * and messages into entries.
   *
   * @tparam TEntryType the type of the entries to be parsed.
   * @return Vector of processed entries.
   */
  template <typename TEntryType>
  std::vector<TEntryType> ProcessMessages();

  /**
   * @brief Returns reference to current set of unconsumed requests.
   * Note: A call to ProcessMessages() is required to parse new requests.
   */
  template <typename TMessageType>
  std::deque<TMessageType>& req_messages() {
    return req_data()->Messages<TMessageType>();
  }
  // TODO(yzhao): req_data() requires traffic_class_.role to be set. But HTTP2 uprobe tracing does
  // not set that. So send_data() is created. Investigate more unified approach.
  template <typename TMessageType>
  const std::deque<TMessageType>& send_data() const {
    return send_data_.Messages<TMessageType>();
  }

  /**
   * @brief Returns reference to current set of unconsumed responses.
   * Note: A call to ProcessMessages() is required to parse new responses.
   */
  template <typename TMessageType>
  std::deque<TMessageType>& resp_messages() {
    return resp_data()->Messages<TMessageType>();
  }
  template <typename TMessageType>
  const std::deque<TMessageType>& recv_data() const {
    return recv_data_.Messages<TMessageType>();
  }

  /**
   * @brief Get the protocol for this connection.
   *
   * @return protocol.
   */
  TrafficProtocol protocol() const { return traffic_class_.protocol; }

  /**
   * @brief Get the role (requestor/responder) for this connection.
   *
   * @return role.
   */
  EndpointRole role() const { return traffic_class_.role; }

  /**
   * Get PID of the connection.
   *
   * @return PID.
   */
  uint64_t pid() const { return conn_id_.upid.pid; }

  /**
   * Get start_time of the PID. Used to disambiguate reusued PIDs.
   *
   * @return start time.
   */
  uint64_t pid_start_time_ticks() const { return conn_id_.upid.start_time_ticks; }

  /**
   * Get FD of the connection.
   *
   * @return FD.
   */
  uint64_t fd() const { return conn_id_.fd; }

  /**
   * Get generation of the connection.
   *
   * @return generation.
   */
  uint32_t generation() const { return conn_id_.generation; }

  /**
   * Get remote IP addr of the connection.
   *
   * @return IP.
   */
  std::string_view remote_addr() const { return open_info_.remote_addr.addr_str; }

  /**
   * Get remote IP addr of the connection.
   *
   * @return IP.
   */
  int remote_port() const { return open_info_.remote_addr.port; }

  /**
   * @brief Get the connection information (e.g. remote IP, port, PID, etc.) for this connection.
   *
   * @return connection information.
   */
  const SocketOpen& conn() const { return open_info_; }

  /**
   * @brief Get the DataStream of sent messages for this connection.
   *
   * @return Data stream of send data.
   */
  const DataStream& send_data() const { return send_data_; }

  /**
   * @brief Get the DataStream of received messages for this connection.
   *
   * @return Data stream of received data.
   */
  const DataStream& recv_data() const { return recv_data_; }

  /**
   * @brief Get the DataStream of requests for this connection.
   *
   * @return Data stream of requests.
   */
  DataStream* req_data();

  /**
   * @brief Get the DataStream of responses for this connection.
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
    return last_update_timestamp_;
  }

  /**
   * @brief Disables the connection tracker. The tracker will drop all its existing data,
   * and also not accept any future data (future data events will be ignored).
   *
   * The tracker will still wait for a Close event to get destroyed.
   */
  void Disable(std::string_view reason = "");
  void set_state(State state, std::string_view reason);

  /**
   * @brief The tracker is disabled and will not produce any new results.
   *
   * @return true if tracker is disabled.
   */
  bool disabled() { return state_ == State::kDisabled; }
  State state() { return state_; }

  /**
   * @brief Check if all events have been received on this stream.
   * Implies that the Close() event has been received as well.
   *
   * @return whether all data events and connection close have been received.
   */
  bool AllEventsReceived() const;

  /**
   * @brief Marks the ConnectionTracker for death.
   *
   * This indicates that the tracker should not receive any further events,
   * otherwise an warning or error will be produced.
   */
  void MarkForDeath(int32_t countdown = kDeathCountdownIters);

  /**
   * @brief Returns true if this tracker has been marked for death.
   *
   * @return true if this tracker is on its death countdown.
   */
  bool IsZombie() const;

  /**
   * @brief Whether this ConnectionTracker can be destroyed.
   * @return true if this ConnectionTracker is a candidate for destruction.
   */
  bool ReadyForDestruction() const;

  /**
   * @brief Updates the any state that changes per iteration on this connection tracker.
   * Should be called once per sampling, after ProcessMessages().
   */
  void IterationPostTick();

  /**
   * @brief Performs any preprocessing that should happen per iteration on this
   * connection tracker.
   * Should be called once per sampling, before ProcessMessages().
   *
   * @param proc_parser Pointer to a proc_parser for access to /proc filesystem.
   * @param connections A map of inodes to endpoint information.
   */
  void IterationPreTick(system::ProcParser* proc_parser,
                        const std::map<int, system::SocketInfo>* connections);

  /**
   * @brief Sets a the duration after which a connection is deemed to be inactive.
   * After becoming inactive, the connection may either (1) have its buffers purged,
   * where any unparsed messages are discarded or (2) be removed entirely from the
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
  static void SetInactivityDuration(std::chrono::seconds duration) {
    inactivity_duration_ = duration;
  }
  static constexpr std::chrono::seconds kDefaultInactivityDuration{300};

  /**
   * @brief Return the currently configured duration, after which a connection is deemed to be
   * inactive.
   */
  static std::chrono::seconds InactivityDuration() { return inactivity_duration_; }

  enum class CountStats {
    kDataEvent = 0,
    kNumCountStats,
  };

  /**
   * Increment a stats event counter for this tracker.
   * @param stat stat selector.
   */
  void IncrementStat(CountStats stat) { ++stats_[static_cast<int>(stat)]; }

  /**
   * Get current value of a stats event counter for this tracker.
   * @param stat stat selector.
   * @return stat count value.
   */
  int64_t Stat(CountStats stat) { return stats_[static_cast<int>(stat)]; }

  /**
   * @brief Number of TransferData() (i.e. PerfBuffer read) calls during which a ConnectionTracker
   * persists after it has been marked for death. We keep ConnectionTrackers alive to catch
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

  /**
   * Initializes protocol state for a protocol (as specified by the message type).
   *
   * Currently, only MySQL needs to keep a protocol state, so it has a specialization,
   * and the general template is empty.
   */
  template <typename TMessageType>
  void InitProtocolState() {}

  /**
   * Returns the protocol state for a protocol (as specified by the message type).
   *
   * This function is only expected to be called in a templated environment, such as in
   * ProcessMessages<mysql::Packet>.
   */
  template <typename TProtocolStateType>
  TProtocolStateType* protocol_state() const {
    return std::get<std::unique_ptr<TProtocolStateType>>(protocol_state_).get();
  }

  template <typename TMessageType>
  void Cleanup() {
    send_data_.CleanupMessages<TMessageType>();
    recv_data_.CleanupMessages<TMessageType>();

    send_data_.CleanupEvents();
    recv_data_.CleanupEvents();
  }

  template <typename TEntryType>
  std::string DebugString(std::string_view prefix = "") const;

 private:
  void AddConnOpenEvent(const conn_event_t& conn_info);
  void AddConnCloseEvent(const close_event_t& close_event);

  template <typename TEntryType>
  std::vector<TEntryType> ProcessMessagesImpl();
  template <typename TMessageType>
  Status ExtractReqResp();
  void SetConnID(struct conn_id_t conn_id);
  void SetTrafficClass(struct traffic_class_t traffic_class);
  void UpdateTimestamps(uint64_t bpf_timestamp);
  void CheckTracker();
  void HandleInactivity();

  // Used to identify the remove endpoint in case the accept/connect was not traced.
  std::unique_ptr<SocketResolver> conn_resolver_ = nullptr;
  bool conn_resolution_failed_ = false;

  struct conn_id_t conn_id_ = {};
  traffic_class_t traffic_class_{kProtocolUnknown, kRoleUnknown};

  SocketOpen open_info_;

  // The data collected by the stream, one per direction.
  DataStream send_data_;
  DataStream recv_data_;

  // --- Start uprobe-based HTTP2 members.

  // Uprobe-based HTTP2 uses a different scheme, where it holds client and server-initiated streams,
  // instead of send and recv messages. As such, we create aliases for HTTP2.
  DataStream& client_streams_ = send_data_;
  DataStream& server_streams_ = recv_data_;

  // For uprobe-based HTTP2 tracing only.
  // Tracks oldest active stream ID for retiring the head of send_data_/recv_data_ deques.
  uint32_t oldest_active_client_stream_id_;
  uint32_t oldest_active_server_stream_id_;

  // Access the appropriate HalfStream object for the given stream ID.
  http2::HalfStream* HalfStreamPtr(uint32_t stream_id, bool write_event);

  // According to the HTTP2 protocol, Stream IDs are incremented by 2.
  // Client-initiated streams use odd IDs, while server-initiated streams use even IDs.
  static constexpr int kHTTP2StreamIDIncrement = 2;

  // --- End uprobe-based HTTP2 members.

  // The connection close info.
  SocketClose close_info_;

  // The timestamp of the last activity on this connection.
  // Recorded as the latest timestamp on a BPF event.
  uint64_t last_bpf_timestamp_ns_ = 0;

  // The timestamp of the last update on this connection which alters the states.
  //
  // Recorded as the latest touch time on the ConnectionTracker.
  // Currently using steady clock, so cannot be used meaningfully for logging real times.
  // This can be changed in the future if required.
  std::chrono::time_point<std::chrono::steady_clock> last_update_timestamp_;

  uint32_t num_send_events_ = 0;
  uint32_t num_recv_events_ = 0;

  State state_ = State::kCollecting;

  inline static std::chrono::seconds inactivity_duration_ = kDefaultInactivityDuration;

  // Iterations before the tracker can be killed.
  int32_t death_countdown_ = -1;

  std::vector<int64_t> stats_ = std::vector<int64_t>(static_cast<int>(CountStats::kNumCountStats));

  /**
   * Connection trackers need to keep a state because there can be information between
   * needed from previous requests/responses needed to parse or render current request.
   * E.g. MySQL keeps a map of previously occurred stmt prepare events as the state such
   * that future stmt execute events can match up with the correct one using stmt_id.
   */
  std::variant<std::monostate, std::unique_ptr<mysql::State>> protocol_state_;
};

}  // namespace stirling
}  // namespace pl
