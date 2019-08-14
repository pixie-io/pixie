#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "src/stirling/http2.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/mysql/mysql.h"
#include "src/stirling/mysql_parse.h"
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

// TODO(oazizi): Convert ReqRespPair to hold unique_ptrs to messages.
// TODO(yzhao/oazizi): Consider use of std::optional to indicate a non-existent request/response.
// Note that using unique_ptrs may make use of std::optional unnecessary.
template <class TMessageType>
struct ReqRespPair {
  TMessageType req_message;
  TMessageType resp_message;
};

struct IPEndpoint {
  std::string ip;
  int port;
};
StatusOr<IPEndpoint> ParseSockAddr(const conn_info_t& conn_info);

/**
 * DataStream is an object that contains the captured data of either send or recv traffic
 * on a connection.
 *
 * Each DataStream contains a container of raw events, and a container of parsed events.
 * Since events are not aligned in any way, may contain only partial messages.
 * Events stay in the raw container until whole messages are parsed out and placed in the
 * container of parsed messaged.
 */
class DataStream {
 public:
  /**
   * @brief Adds a raw (unparsed) chunk of data into the stream.
   * Uses seq_num inside the SocketDataEvent to determine the sequence spot.
   * @param event The data.
   */
  void AddEvent(std::unique_ptr<SocketDataEvent> event);

  /**
   * @brief Parses as many messages as it can from the raw events into the messages container.
   * @tparam TMessageType The parsed message type within the deque.
   * @param type whether to parse as requests, responses or mixed traffic.
   * @return deque of parsed messages.
   */
  template <class TMessageType>
  std::deque<TMessageType>& ExtractMessages(MessageType type);

  /**
   * Returns the current set of parsed messages.
   * @tparam TMessageType The parsed message type within the deque.
   * @return deque of messages.
   */
  template <class TMessageType>
  std::deque<TMessageType>& Messages();

  /**
   * @brief Clears all unparsed and parsed data from the Datastream.
   */
  void Reset();

  /**
   * @brief Checks if the DataStream is empty of both raw events and parsed messages.
   * @return true if empty of all data.
   */
  template <class TMessageType>
  bool Empty() const;

  /**
   * @brief Checks if the DataStream is in a Stuck state, which means that it has
   * raw events, but that it cannot parse any of them.
   *
   * There are two separate stuck cases:
   * 1) Missing Events: a stream is considered stuck if there is any detectable gap in the received
   * events sequence. Any successful parse resets the stuck state. Note, however, that the stream
   * may immediately re-enter a stuck state if a different missing event is discovered after it
   * unblocks.
   *
   * 2) Parse failure of the head: a stream is also considered stuck if the messages at the head
   * cannot be consumed because they cannot be parsed.
   *
   * @return true if DataStream is stuck.
   */
  bool Stuck() { return stuck_count_ != 0; }

  http2::Inflater* Inflater() {
    if (inflater_ == nullptr) {
      inflater_ = std::make_unique<http2::Inflater>();
    }
    return inflater_.get();
  }

 private:
  // Helper function that appends all contiguous events to the parser.
  // Returns number of events appended.
  template <class TMessageType>
  size_t AppendEvents(EventParser<TMessageType>* parser) const;

  // Assuming a stream with an event at the head, attempt to find the next message boundary.
  // Updates seq_num_ and offset_ to the next boundary position.
  template <class TMessageType>
  bool AttemptSyncToMessageBoundary();

  // Raw data events from BPF.
  // TODO(oazizi/yzhao): Convert this to vector or deque.
  std::map<size_t, TimestampedData> events_;

  // Keep track of the sequence number of the stream.
  // This is used to identify missing events.
  size_t next_seq_num_ = 0;

  // To support partially processed events,
  // the stream may start at an offset in the first raw data event.
  uint64_t offset_ = 0;

  // Vector of parsed HTTP/MySQL messages.
  // Once parsed, the raw data events should be discarded.
  // std::variant adds 8 bytes of overhead (to 80->88 for deque)
  //
  // std::variant<> default constructs with the first type parameter. So by default,
  // std::get<> will succeed only for the first type variant, if the variant has not been
  // initialized after definition.
  //
  // Additionally, ConnectionTracker must not switch type during runtime, which indicates serious
  // bug, so we add std::monostate as the default type. And switch to the right time in runtime.
  std::variant<std::monostate, std::deque<http::HTTPMessage>, std::deque<http2::Frame>,
               std::deque<mysql::Packet>>
      messages_;

  // The following state keeps track of whether the raw events were touched or not since the last
  // call to ExtractMessages(). It enables ExtractMessages() to exit early if nothing has changed.
  bool has_new_events_ = false;

  // Number of consecutive calls to ExtractMessages(), where there are a non-zero number of events,
  // but no parsed messages are produced.
  int64_t stuck_count_ = 0;

  // Only meaningful for HTTP2. See also Inflater().
  // TODO(yzhao): We can put this into a std::variant.
  std::unique_ptr<http2::Inflater> inflater_;
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
  void AddDataEvent(std::unique_ptr<SocketDataEvent> event);

  /**
   * @brief Parses raw events in both request and response data streams into messages.
   */
  template <class TMessageType>
  Status ExtractMessages();

  /**
   *
   * @tparam TEntryType
   */
  template <class TEntryType>
  std::vector<TEntryType> ProcessMessages();

  /**
   * @brief Returns reference to current set of unconsumed requests.
   * Note: A call to ExtractMessages() is required to parse new requests.
   */
  template <class TMessageType>
  std::deque<TMessageType>& req_messages() {
    return req_data()->Messages<TMessageType>();
  }

  /**
   * @brief Returns reference to current set of unconsumed responses.
   * Note: A call to ExtractMessages() is required to parse new responses.
   */
  template <class TMessageType>
  std::deque<TMessageType>& resp_messages() {
    return resp_data()->Messages<TMessageType>();
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
  ReqRespRole role() const { return traffic_class_.role; }

  /**
   * Get PID of the connection.
   *
   * @return PID.
   */
  uint64_t pid() const { return conn_id_.pid; }

  /**
   * Get start_time of the PID. Used to disambiguate reusued PIDs.
   *
   * @return start time.
   */
  uint64_t pid_start_time() const { return conn_id_.pid_start_time_ns; }

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
   * Should be called once per sampling (PerfBuffer read).
   */
  void IterationTick();

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

  /**
   * @brief Resets the inactivity duration to the default value.
   *
   * NOTE: This function is static because it is meant to be used for testing purposes only.
   */
  static void SetDefaultInactivityDuration() { SetInactivityDuration(kDefaultInactivityDuration); }

  /**
   * @brief Return the currently configured duration, after which a connection is deemed to be
   * inactive.
   */
  std::chrono::seconds InactivityDuration() { return inactivity_duration_; }

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
   * Curretly, only MySQL needs to keep a state, so it has a specialized template of
   * the InitState function, and the general template is empty.
   */
  template <class TMessageType>
  void InitState() {}

  /**
   * state() gets the state of a connection tracker. This function is only expected to
   * be called in a templated environment, such as in ProcessMessages<mysql::Packet>.
   */
  template <class TStateType>
  TStateType* state() const {
    return std::get<std::unique_ptr<TStateType>>(state_).get();
  }

 private:
  void SetPID(struct conn_id_t conn_id);
  void SetTrafficClass(struct traffic_class_t traffic_class);
  void UpdateTimestamps(uint64_t bpf_timestamp);
  void HandleInactivity();

  struct conn_id_t conn_id_ {
    {0}, {0}, 0, 0
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

  static constexpr std::chrono::seconds kDefaultInactivityDuration{300};
  inline static std::chrono::seconds inactivity_duration_ = kDefaultInactivityDuration;

  // Iterations before the tracker can be killed.
  int32_t death_countdown_ = -1;

  std::vector<int64_t> stats_ = std::vector<int64_t>(static_cast<int>(CountStats::kNumCountStats));

  /**
   * Connection trackers need to keep a state because there can be information between
   * needed from previous requests/responses needed to parse or render current request.
   * E.g. MySQL keeps a map of previously occured stmt prepare events as the state such
   * that future stmt execute events can match up with the correct one using stmt_id.
   */
  std::variant<std::monostate, std::unique_ptr<std::map<int, mysql::ReqRespEvent>>> state_;
};

}  // namespace stirling
}  // namespace pl
