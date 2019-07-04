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
struct SocketConnection {
  uint64_t timestamp_ns = 0;
  uint32_t tgid = 0;
  uint32_t fd = -1;
  std::string remote_addr = "-";
  int remote_port = -1;
};

struct SocketClose {
  bool closed = false;
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
  // TODO(oazizi): Convert this to vector.
  std::map<uint64_t, SocketDataEvent> events;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset = 0;

  // Vector of parsed HTTP messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  std::variant<std::deque<HTTPMessage>, std::deque<http2::Frame> > messages;

  /**
   * @ brief Parses as many messages as it can from the raw events into the messages container.
   *
   * @param type whether to parse as requests, responses or mixed traffic.
   */
  template <class TMessageType>
  void ExtractMessages(MessageType type);

  // TODO(oazizi): Add a bool to say whether the stream has been touched since last transfer (to
  // avoid useless computation in ExtractMessages()).

  /**
   * @ brief Parses as many messages as it can from the raw events into the messages container.
   *
   * @param type whether to parse as requests, responses or mixed traffic.
   */
  void ExtractMessages(MessageType type);
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
   * @brief Get the connection information (e.g. remote IP, port, PID, etc.) for this connection.
   *
   * @return connection information.
   */
  const SocketConnection& conn() const { return conn_; }

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
   * @brief Check if all events have been received on this stream.
   * Implies that the Close() event has been received as well.
   *
   * @return whether all data events and connection close have been received.
   */
  bool AllEventsReceived() const;

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

 private:
  void UpdateTimestamps(uint64_t bpf_timestamp);

  traffic_class_t traffic_class_{kProtocolUnknown, kRoleUnknown};

  SocketConnection conn_;

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

  void SetTrafficClass(struct traffic_class_t traffic_class);

  // TODO(oazizi): Could record a timestamp, so we could destroy old EventStreams completely.
};

}  // namespace stirling
}  // namespace pl
