#pragma once

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <variant>

#include "src/stirling/bcc_bpf/socket_trace.h"
#include "src/stirling/http_parse.h"

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
  std::map<uint64_t, socket_data_event_t> events;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset = 0;

  // Vector of parsed HTTP messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  std::variant<std::deque<HTTPMessage> > messages;

  /**
   * @ brief Parses as many messages as it can from the raw events into the messages container.
   *
   * @param type whether to parse as requests, responses or mixed traffic.
   */
  template <class TMessageType>
  void ExtractMessages(TrafficMessageType type);

  // TODO(oazizi): Add a bool to say whether the stream has been touched since last transfer (to
  // avoid useless computation in ExtractMessages()).

  /**
   * @ brief Parses as many messages as it can from the raw events into the messages container.
   *
   * @param type whether to parse as requests, responses or mixed traffic.
   */
  void ExtractMessages(TrafficMessageType type);
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
  void AddConnCloseEvent();

  /**
   * @brief Registers a BPF data event into the tracker.
   *
   * @param event The data event from BPF.
   */
  void AddDataEvent(socket_data_event_t event);

  /**
   * @brief Get the protocol for this connection.
   *
   * @return protocol.
   */
  TrafficProtocol protocol() const { return protocol_; }

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
  DataStream& send_data() { return send_data_; }

  /**
   * @brief Get the DataStream of received messages for this connection.
   *
   * @return Data stream of received data.
   */
  const DataStream& recv_data() const { return recv_data_; }
  DataStream& recv_data() { return recv_data_; }

  // TODO(oazizi): Clean-up accessors above.
  //  - Some of the accessors are only for testing purposes.
  //  - May want to give deeper access to certain substructures inside DataStream instead of the
  //  whole stream.

 protected:
  TrafficProtocol protocol_;

 private:
  SocketConnection conn_;

  // Whether the connection close() event has been observed.
  bool closed_ = false;

  // The data collected by the stream, one per direction.
  DataStream send_data_;
  DataStream recv_data_;

  void SetProtocol(TrafficProtocol protocol);

  // TODO(oazizi): Could record a timestamp, so we could destroy old EventStreams completely.
};

}  // namespace stirling
}  // namespace pl
