#pragma once

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <variant>

#include "src/stirling/http2.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/socket_trace.h"

namespace pl {
namespace stirling {

/**
 * @brief Describes a connection from user space. This corresponds to struct conn_info_t in
 * src/stirling/bcc_bpf/socket_trace.h.
 */
struct SocketOpen {
  uint64_t timestamp_ns = 0;
  std::string remote_addr = "-";
  int remote_port = -1;
};

struct SocketClose {
  uint64_t timestamp_ns = 0;
  // The send/write sequence number at time of close.
  uint64_t send_seq_num = 0;
  // The recv/read sequence number at time of close.
  uint64_t recv_seq_num = 0;
};

/**
 * DataStream is an object that contains the captured data of either send or recv traffic
 * on a connection.
 *
 * Each DataStream contains a container of raw events, and a container of parsed events.
 * Since events are not aligned in any way, may contain only partial messages.
 * Events stay in the raw container until whole messages are parsed out and placed in the
 * container of parsed messaged.
 */
// TODO(oazizi): Turn this into a class.
struct DataStream {
  // Raw data events from BPF.
  // TODO(oazizi/yzhao): Convert this to vector or deque.
  std::map<uint64_t, SocketDataEvent> events;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset = 0;

  // Vector of parsed HTTP messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  //
  // std::variant<> default constructs with the first type parameter. So by default,
  // std::get<> will succeed only for the first type variant, if the variant has not been
  // initialized after definition.
  //
  // Additionally, ConnectionTracker must not switch type during runtime, which indicates serious
  // bug, so we add std::monostate as the default type. And switch to the right time in runtime.
  std::variant<std::monostate, std::deque<HTTPMessage>, std::deque<http2::Frame> > messages;

  /**
   * @ brief Parses as many messages as it can from the raw events into the messages container.
   *
   * @param type whether to parse as requests, responses or mixed traffic.
   */
  template <class TMessageType>
  void ExtractMessages(MessageType type);

  // TODO(oazizi): Add a bool to say whether the stream has been touched since last transfer (to
  // avoid useless computation in ExtractMessages()).
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
  /**
   * @brief Registers a BPF connection open event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddConnOpenEvent(conn_info_t conn_info);

  /**
   * @brief Registers a BPF connection close event into the tracker.
   */
  void AddConnCloseEvent(conn_info_t conn_info);

  /**
   * @brief Registers a BPF data event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddDataEvent(SocketDataEvent event);

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
  ReqRespRole role() const { return traffic_class_.role; }

  /**
   * Get PID of the connection.
   *
   * @return PID.
   */
  uint64_t pid() const { return conn_id_.tgid; }

  /**
   * Get start_time of the PID. Used to disambiguate reusued PIDs.
   *
   * @return start time.
   */
  uint64_t pid_start_time() const { return conn_id_.tgid_start_time_ns; }

  /**
   * Get FD of the connection.
   *
   * @return FD.
   */
  uint64_t fd() const { return conn_id_.fd; }

  /**
   * Get remote IP addr of the connection.
   *
   * @return IP.
   */
  std::string_view remote_addr() const { return open_info_.remote_addr; }

  /**
   * Get remote IP addr of the connection.
   *
   * @return IP.
   */
  int remote_port() const { return open_info_.remote_port; }

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
  void MarkForDeath();

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
   * Should be called once per sampling (PerfBuffer read).
   */
  void IterationTick();

  /**
   * @brief Number of TransferData() (i.e. PerfBuffer read) calls any ConnectionTracker persists
   * after it has been marked for death. We keep ConnectionTrackers alive for debug purposes only,
   * just to log spurious late events (which should not happen).
   */
  static constexpr int64_t kDeathCountdownIters = 2;

 private:
  void SetPID(struct conn_id_t conn_id);
  void SetTrafficClass(struct traffic_class_t traffic_class);
  void UpdateTimestamps(uint64_t bpf_timestamp);

  struct conn_id_t conn_id_ {
    0, 0, 0, 0
  };
  traffic_class_t traffic_class_{kProtocolUnknown, kRoleUnknown};

  SocketOpen open_info_;

  // The data collected by the stream, one per direction.
  DataStream send_data_;
  DataStream recv_data_;

  // The connection close info.
  SocketClose close_info_;

  // The timestamp of the last activity on this connection.
  // Recorded as the latest timestamp on a BPF event.
  uint64_t last_bpf_timestamp_ns_ = 0;

  // The timestamp of the last activity on this connection.
  // Recorded as the latest touch time on the ConnectionTracker.
  // Currently using steady clock, so cannot be used meaningfully for logging real times.
  // This can be changed in the future if required.
  std::chrono::time_point<std::chrono::steady_clock> last_update_timestamp_;

  uint32_t num_send_events_ = 0;
  uint32_t num_recv_events_ = 0;

  // Iterations before the tracker can be killed.
  int64_t death_countdown_ = -1;

  // TODO(oazizi): Could record a timestamp, so we could destroy old EventStreams completely.
};

}  // namespace stirling
}  // namespace pl
