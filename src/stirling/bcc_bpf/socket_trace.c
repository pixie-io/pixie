// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <linux/errno.h>
#include <linux/in6.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/socket.h>

#include "src/stirling/bcc_bpf_interface/grpc.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"

#include "src/stirling/bcc_bpf/utils.h"

// TODO(yzhao): Investigate the performance overhead of active_*_info_map.delete(id), when id is not
// in the map. If it's significant, change to only call delete() after knowing that id is in the
// map.

// This keeps instruction count below BPF's limit of 4096 per probe.
// TODO(yzhao): Investigate using tail call to reuse stack space to support loop.
// TODO(yzhao): Optimize the code to remove unneeded code, and increase the loop count.
// TODO(PL-914): 4.13 and older kernel versions need smaller number, 10 is tested to work.
// See the referenced Jira issue for more details.
#define LOOP_LIMIT 35

// Determines what percentage of events must be inferred as a certain type for us to consider the
// connection to be of that type. Encoded as a numerator/denominator. Currently set to 20%. While
// this may seem low, one must consider that not all captures are packet-aligned, and the inference
// logic doesn't work on the middle of packets. Moreover, a large data packet would get split up
// and cause issues. This threshold only needs to be larger than the false positive rate, which
// for MySQL is 32/256 based on command only.
const int kTrafficInferenceThresholdNum = 1;
const int kTrafficInferenceThresholdDen = 5;

// This bias is added to the numerator of the traffic inference threshold.
// By using a positive number, it biases messages to be classified as matches,
// when the number of samples is low.
const int kTrafficInferenceBias = 5;

// This is the perf buffer for BPF program to export data from kernel to user space.
BPF_PERF_OUTPUT(socket_data_events);
BPF_PERF_OUTPUT(socket_control_events);

/***********************************************************
 * Internal structs and definitions
 ***********************************************************/

struct connect_info_t {
  struct sockaddr_in6 addr;
  // TODO(PL-693): Use bpf_getsockopt() to detect socket type and address family. So we can unify
  // the entry probes of accept() & connect().
  u32 fd;
} __attribute__((__packed__, aligned(8)));

struct accept_info_t {
  struct sockaddr* addr;
  // TODO(yzhao): This is not used, remove.
  size_t* addrlen;
} __attribute__((__packed__, aligned(8)));

struct data_info_t {
  // The timestamp of the read/write syscall entry.
  uint64_t timestamp_ns;
  u32 fd;
  const char* buf;
  // For sendmsg()/recvmsg()/writev()/readv().
  const struct iovec* iov;
  size_t iovlen;
} __attribute__((__packed__, aligned(8)));

struct close_info_t {
  u32 fd;
} __attribute__((__packed__, aligned(8)));

// This control_map is a bit-mask that controls which endpoints are traced in a connection.
// The bits are defined in EndpointRole enum, kRoleClient or kRoleServer. kRoleUnknown is not
// really used, but is defined for completeness.
// There is a control map element for each protocol.
BPF_PERCPU_ARRAY(control_map, u64, kNumProtocols);

// Map from user-space file descriptors to the connections obtained from accept() syscall.
// Tracks connection from accept() -> close().
// Key is {tgid, fd}.
BPF_HASH(conn_info_map, u64, struct conn_info_t);

// Map from user-space file descriptors to open files obtained from open() syscall.
// Used to filter out file read/writes.
// Tracks connection from open() -> close().
// Key is {tgid, fd}.
BPF_HASH(open_file_map, u64, bool);

// Map from thread to its ongoing accept() syscall's input argument.
// Tracks accept() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_accept_info_map, u64, struct accept_info_t);

// Map from thread to its ongoing connect() syscall's input argument.
// Tracks connect() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_connect_info_map, u64, struct connect_info_t);

// Map from thread to its ongoing write() syscall's input argument.
// Tracks write() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_write_info_map, u64, struct data_info_t);

// Map from thread to its ongoing read() syscall's input argument.
// Tracks read() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_read_info_map, u64, struct data_info_t);

// Map from thread to its ongoing close() syscall's input argument.
// Tracks close() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_close_info_map, u64, struct close_info_t);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(data_buffer_heap, struct socket_data_event_t, 1);

// This array records singular values that are used by probes. We group them together to reduce the
// number of arrays with only 1 element.
BPF_PERCPU_ARRAY(control_values, s64, kNumControlValues);

/***********************************************************
 * General helper functions
 ***********************************************************/

static __inline u64 gen_tgid_fd(u32 tgid, int fd) { return ((u64)tgid << 32) | (u32)fd; }

static __inline void set_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  bool kTrue = 1;
  open_file_map.insert(&tgid_fd, &kTrue);
}

static __inline bool is_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  bool* open_file = open_file_map.lookup(&tgid_fd);
  return (open_file != NULL);
}

static __inline void clear_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  open_file_map.delete(&tgid_fd);
}

// The caller must memset conn_info to '0', otherwise the behavior is undefined.
static __inline void init_conn_info(u32 tgid, u32 fd, struct conn_info_t* conn_info) {
  conn_info->conn_id.upid.tgid = tgid;
  conn_info->conn_id.upid.start_time_ticks = get_tgid_start_time();
  conn_info->conn_id.fd = fd;
  conn_info->conn_id.tsid = bpf_ktime_get_ns();
}

static __inline struct conn_info_t* get_conn_info(u32 tgid, u32 fd) {
  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  struct conn_info_t new_conn_info;
  memset(&new_conn_info, 0, sizeof(struct conn_info_t));
  new_conn_info.addr_valid = false;
  struct conn_info_t* conn_info = conn_info_map.lookup_or_init(&tgid_fd, &new_conn_info);
  // Use TGID zero to detect that a new conn_info needs to be initialized.
  if (conn_info->conn_id.upid.tgid == 0) {
    init_conn_info(tgid, fd, conn_info);
  }
  return conn_info;
}

/***********************************************************
 * Trace filtering functions
 ***********************************************************/

static __inline bool should_trace_protocol(const struct traffic_class_t* traffic_class) {
  u32 protocol = traffic_class->protocol;

  // If this connection has an unknown protocol, abort (to avoid pollution).
  // TODO(oazizi/yzhao): Remove this after we implement connections of interests through tgid + fd.
  // NOTE: This check is covered by control_map below too, but keeping it around for explicitness.
  if (protocol == kProtocolUnknown) {
    return false;
  }

  u64 kZero = 0;
  // TODO(yzhao): BCC doc states BPF_PERCPU_ARRAY: all array elements are **pre-allocated with zero
  // values** (https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md). That suggests
  // lookup() suffices. But this seems more robust, as BCC behavior is often not intuitive.
  u64 control = *control_map.lookup_or_init(&protocol, &kZero);
  return control & traffic_class->role;
}

// Returns true if detection passes threshold. Right now this only makes sense for MySQL.
static __inline bool protocol_detection_passes_threshold(const struct conn_info_t* conn_info) {
  if (conn_info->traffic_class.protocol == kProtocolMySQL) {
    // Since some protocols are hard to infer from a single event, we track the inference stats over
    // time, and then use the match rate to determine whether we really want to consider it to be of
    // the protocol or not. This helps reduce polluting events to user-space.
    bool meets_threshold =
        kTrafficInferenceThresholdDen * (conn_info->protocol_match_count + kTrafficInferenceBias) >
        kTrafficInferenceThresholdNum * conn_info->protocol_total_count;
    return meets_threshold;
  }
  return true;
}

static __inline bool should_trace_conn(const struct conn_info_t* conn_info) {
  return protocol_detection_passes_threshold(conn_info) &&
         should_trace_protocol(&conn_info->traffic_class);
}

static __inline bool test_only_should_trace_target_tgid(const u32 tgid) {
  int idx = kTargetTGIDIndex;
  s64* target_tgid = control_values.lookup(&idx);
  if (target_tgid == NULL) {
    return true;
  }
  if (*target_tgid < 0) {
    return true;
  }
  return *target_tgid == tgid;
}

static __inline bool is_stirling_tgid(const u32 tgid) {
  int idx = kStirlingTGIDIndex;
  s64* target_tgid = control_values.lookup(&idx);
  if (target_tgid == NULL) {
    return false;
  }
  return *target_tgid == tgid;
}

static __inline bool should_trace_tgid(const u32 tgid) {
  return test_only_should_trace_target_tgid(tgid) && !is_stirling_tgid(tgid);
}

static __inline struct socket_data_event_t* fill_event(enum TrafficDirection direction,
                                                       const struct data_info_t* data_info,
                                                       const struct conn_info_t* conn_info) {
  u32 kZero = 0;
  struct socket_data_event_t* event = data_buffer_heap.lookup(&kZero);
  if (event == NULL) {
    return NULL;
  }
  event->attr.entry_timestamp_ns = data_info->timestamp_ns;
  event->attr.return_timestamp_ns = bpf_ktime_get_ns();
  event->attr.direction = direction;
  event->attr.conn_id = conn_info->conn_id;
  event->attr.traffic_class = conn_info->traffic_class;
  return event;
}

/***********************************************************
 * Traffic inference helper functions
 ***********************************************************/

static __inline enum MessageType infer_http_message(const char* buf, size_t count) {
  // Smallest HTTP response is 17 characters:
  // HTTP/1.1 200 OK\r\n
  // Smallest HTTP response is 16 characters:
  // GET x HTTP/1.1\r\n
  if (count < 16) {
    return kUnknown;
  }

  if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
    return kResponse;
  }
  if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
    return kRequest;
  }
  if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
    return kRequest;
  }
  // TODO(oazizi): Should we add PUT, DELETE, HEAD, and perhaps others?

  return kUnknown;
}

// MySQL packet:
//      0         8        16        24        32
//      +---------+---------+---------+---------+
//      |        payload_length       | seq_id  |
//      +---------+---------+---------+---------+
//      |                                       |
//      .            ...  body ...              .
//      .                                       .
//      .                                       .
//      +----------------------------------------
// TODO(oazizi/yzhao): This produces too many false positives. Add stronger protocol detection.
static __inline enum MessageType infer_mysql_message(const uint8_t* buf, size_t count) {
  static const uint8_t kComQuery = 0x03;
  static const uint8_t kComConnect = 0x0b;
  static const uint8_t kComStmtPrepare = 0x16;
  static const uint8_t kComStmtExecute = 0x17;
  static const uint8_t kComStmtClose = 0x19;

  // MySQL packets start with a 3-byte packet length and a 1-byte packet number.
  // The 5th byte on a request contains a command that tells the type.
  if (count < 5) {
    return kUnknown;
  }

  // Convert 3-byte length to uint32_t.
  // NOLINTNEXTLINE: readability/casting
  uint32_t len = *((uint32_t*)buf);
  len = len & 0x00ffffff;

  uint8_t seq = buf[3];
  uint8_t com = buf[4];

  // The packet number of a request should always be 0.
  if (seq != 0) {
    return kUnknown;
  }

  // No such thing as a zero-length request in MySQL protocol.
  if (len == 0) {
    return kUnknown;
  }

  // Assuming that the length of a request is less than 10k characters to avoid false
  // positive flagging as MySQL, which statistically happens frequently for a single-byte
  // check.
  if (len > 10000) {
    return kUnknown;
  }

  // TODO(oazizi): Consider adding more commands (0x00 to 0x1f).
  // Be careful, though: trade-off is higher rates of false positives.
  if (com == kComConnect || com == kComQuery || com == kComStmtPrepare || com == kComStmtExecute ||
      com == kComStmtClose) {
    return kRequest;
  }
  return kUnknown;
}

// Cassandra frame:
//      0         8        16        24        32         40
//      +---------+---------+---------+---------+---------+
//      | version |  flags  |      stream       | opcode  |
//      +---------+---------+---------+---------+---------+
//      |                length                 |
//      +---------+---------+---------+---------+
//      |                                       |
//      .            ...  body ...              .
//      .                                       .
//      .                                       .
//      +----------------------------------------
static __inline enum MessageType infer_cql_message(const uint8_t* buf, size_t count) {
  static const uint8_t kError = 0x00;
  static const uint8_t kStartup = 0x01;
  static const uint8_t kReady = 0x02;
  static const uint8_t kAuthenticate = 0x03;
  static const uint8_t kOptions = 0x05;
  static const uint8_t kSupported = 0x06;
  static const uint8_t kQuery = 0x07;
  static const uint8_t kResult = 0x08;
  static const uint8_t kPrepare = 0x09;
  static const uint8_t kExecute = 0x0a;
  static const uint8_t kRegister = 0x0b;
  static const uint8_t kEvent = 0x0c;
  static const uint8_t kBatch = 0x0d;
  static const uint8_t kAuthChallenge = 0x0e;
  static const uint8_t kAuthResponse = 0x0f;
  static const uint8_t kAuthSuccess = 0x10;

  // Cassandra frames have a 9-byte header.
  if (count < 9) {
    return kUnknown;
  }

  // Version contains both version and direction.
  bool request = (buf[0] & 0x80) == 0x00;
  uint8_t version = (buf[0] & 0x7f);
  uint8_t flags = buf[1];
  uint8_t opcode = buf[4];
  int32_t length;
  bpf_probe_read(&length, 4, &buf[5]);
  length = bpf_ntohl(length);

  // Cassandra version should 5 or less. Also v2 and lower seem much less popular.
  // For example ScyllaDB only supports v3+.
  if (version < 3 || version > 5) {
    return kUnknown;
  }

  // Only flags 0x1, 0x2, 0x4 and 0x8 are used.
  if ((flags & 0xf0) != 0) {
    return kUnknown;
  }

  // A frame is limited to 256MB in length,
  // but we look for more common frames which should be much smaller in size.
  if (length > 10000) {
    return false;
  }

  switch (opcode) {
    case kStartup:
    case kOptions:
    case kQuery:
    case kPrepare:
    case kExecute:
    case kRegister:
    case kBatch:
    case kAuthResponse:
      return request ? kRequest : kUnknown;
    case kError:
    case kReady:
    case kAuthenticate:
    case kSupported:
    case kResult:
    case kEvent:
    case kAuthChallenge:
    case kAuthSuccess:
      return !request ? kResponse : kUnknown;
    default:
      return kUnknown;
  }
}

static __inline enum MessageType infer_http2_message(const char* buf, size_t count) {
  // Technically, HTTP2 client connection preface is 24 octets [1]. Practically,
  // the first 3 shall be sufficient. Note this is sent from client,
  // so it would be captured on server's read()/recvfrom()/recvmsg() or client's
  // write()/sendto()/sendmsg().
  //
  // [1] https://http2.github.io/http2-spec/#ConnectionHeader
  if (count < 3) {
    return kUnknown;
  }

  if (buf[0] == 'P' && buf[1] == 'R' && buf[2] == 'I') {
    return kRequest;
  }

  if (looks_like_grpc_req_http2_headers_frame(buf, count)) {
    return kRequest;
  }

  return kUnknown;
}

static __inline struct traffic_class_t infer_traffic(enum TrafficDirection direction,
                                                     const char* buf, size_t count) {
  struct traffic_class_t traffic_class;
  traffic_class.protocol = kProtocolUnknown;
  traffic_class.role = kRoleUnknown;

  enum MessageType req_resp_type;
  if ((req_resp_type = infer_http_message(buf, count)) != kUnknown) {
    traffic_class.protocol = kProtocolHTTP;
  } else if ((req_resp_type = infer_cql_message(buf, count)) != kUnknown) {
    traffic_class.protocol = kProtocolCQL;
  } else if ((req_resp_type = infer_mysql_message(buf, count)) != kUnknown) {
    traffic_class.protocol = kProtocolMySQL;
  } else if ((req_resp_type = infer_http2_message(buf, count)) != kUnknown) {
    traffic_class.protocol = kProtocolHTTP2;
  } else {
    return traffic_class;
  }

  // Classify Role as XOR between direction and req_resp_type:
  //    direction  req_resp_type  => role
  //    ------------------------------------
  //    kEgress    kRequest       => Client
  //    kEgress    KResponse      => Server
  //    kIngress   kRequest       => Server
  //    kIngress   kResponse      => Client
  traffic_class.role =
      ((direction == kEgress) ^ (req_resp_type == kResponse)) ? kRoleClient : kRoleServer;
  return traffic_class;
}

// TODO(oazizi): This function should go away once the protocol is identified externally.
//               Also, could move this function into the header file, so we can test it.
static __inline void update_traffic_class(struct conn_info_t* conn_info,
                                          enum TrafficDirection direction, const char* buf,
                                          size_t count) {
  // TODO(oazizi): Future architecture should have user-land provide the traffic_class.
  // TODO(oazizi): conn_info currently works only if tracing on the send or recv side of a process,
  //               but not both simultaneously, because we need to mark two traffic classes.

  if (conn_info != NULL) {
    // Try to infer connection type (protocol) based on data.
    struct traffic_class_t traffic_class = infer_traffic(direction, buf, count);

    if (conn_info->traffic_class.protocol == kProtocolUnknown) {
      // TODO(oazizi): Look for only certain protocols on write/send()?
      conn_info->traffic_class = traffic_class;

      if (conn_info->traffic_class.protocol != kProtocolUnknown) {
        conn_info->protocol_match_count = 1;
      }
    } else if (traffic_class.protocol == conn_info->traffic_class.protocol) {
      conn_info->protocol_match_count += 1;
    }

    conn_info->protocol_total_count += 1;
  }
}

/***********************************************************
 * Perf submit functions
 ***********************************************************/

static __inline void submit_new_conn(struct pt_regs* ctx, u32 tgid, u32 fd,
                                     struct sockaddr_in6 addr) {
  struct conn_info_t conn_info;
  memset(&conn_info, 0, sizeof(struct conn_info_t));
  conn_info.addr_valid = true;
  conn_info.addr = addr;
  init_conn_info(tgid, fd, &conn_info);

  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  conn_info_map.update(&tgid_fd, &conn_info);

  struct socket_control_event_t conn_event;
  memset(&conn_event, 0, sizeof(struct socket_control_event_t));
  conn_event.type = kConnOpen;
  conn_event.open.timestamp_ns = bpf_ktime_get_ns();
  conn_event.open.conn_id = conn_info.conn_id;
  conn_event.open.addr = conn_info.addr;
  conn_event.open.traffic_class = conn_info.traffic_class;

  socket_control_events.perf_submit(ctx, &conn_event, sizeof(struct socket_control_event_t));
}

static __inline void submit_close_event(struct pt_regs* ctx, struct conn_info_t* conn_info) {
  struct socket_control_event_t close_event;
  memset(&close_event, 0, sizeof(struct socket_control_event_t));
  close_event.type = kConnClose;
  close_event.close.timestamp_ns = bpf_ktime_get_ns();
  close_event.close.conn_id = conn_info->conn_id;
  close_event.close.rd_seq_num = conn_info->rd_seq_num;
  close_event.close.wr_seq_num = conn_info->wr_seq_num;

  socket_control_events.perf_submit(ctx, &close_event, sizeof(struct socket_control_event_t));
}

// TODO(yzhao): We can write a test for this, by define a dummy bpf_probe_read() function. Similar
// in idea to a mock in normal C++ code.
//
// Writes the input buf to event, and submits the event to the corresponding perf buffer.
//
// Returns the bytes output from the input buf. Note that is not the total bytes submitted to the
// perf buffer, which includes additional metadata.
static __inline size_t perf_submit_buf(struct pt_regs* ctx, const enum TrafficDirection direction,
                                       const char* buf, const size_t buf_size,
                                       struct conn_info_t* conn_info,
                                       struct socket_data_event_t* event) {
  switch (direction) {
    case kEgress:
      event->attr.seq_num = conn_info->wr_seq_num;
      ++conn_info->wr_seq_num;
      break;
    case kIngress:
      event->attr.seq_num = conn_info->rd_seq_num;
      ++conn_info->rd_seq_num;
      break;
  }

  // This part of the code has been written carefully to keep the BPF verifier happy in older
  // kernels, so please take care when modifying.
  //
  // Logically, what we'd like is the following:
  //    size_t msg_size = buf_size < sizeof(event->msg) ? buf_size : sizeof(event->msg);
  //    bpf_probe_read(&event->msg, msg_size, buf);
  //    event->attr.msg_size = buf_size;
  //    socket_data_events.perf_submit(ctx, event, size_to_submit);
  //
  // But this does not work in kernel versions 4.14 or older, for various reasons:
  //  1) the verifier does not like a bpf_probe_read with size 0.
  //       - Useful link: https://www.mail-archive.com/netdev@vger.kernel.org/msg199918.html
  //  2) the verifier does not like a perf_submit that is larger than sizeof(event).
  //
  // While it is often obvious to us humans that these are not problems,
  // the older verifiers can't prove it to themselves.
  //
  // We often try to provide hints to the verifier using approaches like
  // 'if (msg_size > 0)' around the code, but it turns out that clang is often smarter
  // than the verifier, and optimizes away the structural hints we try to provide the verifier.
  //
  // Solution below involves using a volatile asm statement to prevent clang from optimizing away
  // certain code, so that code can reach the BPF verifier, and convince it that everything is
  // safe.
  //
  // Tested to work on GKE node with 4.14.127+ kernel.

  unsigned int msg_submit_size = buf_size;

  // Clang is too smart for us, and tries to remove some of the obvious hints we are leaving for the
  // BPF verifier. So we add this NOP volatile statement, so clang can't optimize away some of our
  // if-statements below.
  // By telling clang that msg_submit_size is both an input and output to some black box assembly
  // code, clang has to discard any assumptions on what values this variable can take.
  asm volatile("" : "+r"(msg_submit_size) :);

  if (msg_submit_size > MAX_MSG_SIZE) {
    // Impossible to enter here, but this helps the BPF verifier.
    return 0;
  }

  bpf_probe_read(&event->msg, msg_submit_size, buf);
  event->attr.msg_size = buf_size;

  // Write data to perf ring buffer.
  unsigned int size_to_submit = sizeof(event->attr) + msg_submit_size;

  // Another effective NOP, but we mask like this to help BPF verifier.
  // Needs to be large enough to encompass the bit-range of MAX_MSG_SIZE.
  // The currently selected value should be safe for all future values, as it is quite large.
  // Actual value is not that important, as it just proves to verifier that
  // size_to_submit is not negative.
  size_to_submit &= 0xfffffff;

  // This if statement should always be true, but required for BPF verifier.
  if (size_to_submit <= sizeof(event->attr) + MAX_MSG_SIZE) {
    socket_data_events.perf_submit(ctx, event, size_to_submit);
  }

  return msg_submit_size;
}

static __inline void perf_submit_wrapper(struct pt_regs* ctx, const enum TrafficDirection direction,
                                         const char* buf, const size_t buf_size,
                                         struct conn_info_t* conn_info,
                                         struct socket_data_event_t* event) {
  int bytes_remaining = buf_size;
  unsigned int i;

#pragma unroll
  for (i = 0; i < CHUNK_LIMIT; ++i) {
    if (bytes_remaining >= MAX_MSG_SIZE) {
      perf_submit_buf(ctx, direction, buf + i * MAX_MSG_SIZE, MAX_MSG_SIZE, conn_info, event);
      bytes_remaining = bytes_remaining - MAX_MSG_SIZE;
    } else if (bytes_remaining > 0) {
      perf_submit_buf(ctx, direction, buf + i * MAX_MSG_SIZE, bytes_remaining, conn_info, event);
      bytes_remaining = 0;
    }
  }

  // If the message is too long, then we can't transmit it all.
  // To indicate to user-space that there is a gap in data transmitted,
  // we increment the appropriate sequence numbers.
  // This will appear as missing data in the socket_trace_connector,
  // which is exactly what we want it to believe.
  if (bytes_remaining > 0) {
    switch (direction) {
      case kEgress:
        ++conn_info->wr_seq_num;
        break;
      case kIngress:
        ++conn_info->rd_seq_num;
        break;
    }
  }
}

static __inline void perf_submit_iovecs(struct pt_regs* ctx, const enum TrafficDirection direction,
                                        const struct iovec* iov, const size_t iovlen,
                                        const size_t total_size, struct conn_info_t* conn_info,
                                        struct socket_data_event_t* event) {
  // NOTE: The loop index 'i' used to be int. BPF verifier somehow conclude that msg_size inside
  // perf_submit_buf(), after a series of assignment, and passed into a function call, can be
  // negative.
  //
  // The issue can be fixed by changing the loop index, or msg_size inside
  // perf_submit_buf(), to unsigned int (changing to size_t does not work either).
  //
  // We prefer changing loop index, as it appears to be the source of triggering BPF verifier's
  // confusion.
  //
  // NOTE: The syscalls for scatter buffers, {send,recv}msg()/{write,read}v(), access buffers in
  // array order. That means they read or fill iov[0], then iov[1], and so on. They return the total
  // size of the written or read data. Therefore, when loop through the buffers, both the number of
  // buffers and the total size need to be checked. More details can be found on their man pages.
#pragma unroll
  for (unsigned int i = 0, bytes_copied = 0;
       i < LOOP_LIMIT && i < iovlen && bytes_copied < total_size; ++i) {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &iov[i]);

    const size_t bytes_remain = total_size - bytes_copied;
    const size_t iov_size = iov_cpy.iov_len < bytes_remain ? iov_cpy.iov_len : bytes_remain;

    // TODO(oazizi/yzhao): Should switch this to go through perf_submit_wrapper.
    const size_t submit_size =
        perf_submit_buf(ctx, direction, iov_cpy.iov_base, iov_size, conn_info, event);
    bytes_copied += submit_size;
  }
}

/***********************************************************
 * BPF syscall probe functions
 ***********************************************************/

static __inline void probe_entry_connect_impl(struct pt_regs* ctx, u64 id, int sockfd,
                                              const struct sockaddr* addr, size_t addrlen) {
  if (sockfd < 0) {
    return;
  }

  u32 tgid = id >> 32;

  if (!should_trace_tgid(tgid)) {
    return;
  }

  // Only record IP (IPV4 and IPV6) connections.
  if (!(addr->sa_family == AF_INET || addr->sa_family == AF_INET6)) {
    return;
  }

  struct connect_info_t connect_info;
  memset(&connect_info, 0, sizeof(struct connect_info_t));
  connect_info.fd = sockfd;
  bpf_probe_read(&connect_info.addr, sizeof(struct sockaddr_in6), (const void*)addr);

  active_connect_info_map.update(&id, &connect_info);
}

static __inline void probe_ret_connect_impl(struct pt_regs* ctx, u64 id) {
  int ret_val = PT_REGS_RC(ctx);
  // We allow EINPROGRESS to go through, which indicates that a NON_BLOCK socket is undergoing
  // handshake.
  //
  // In case connect() eventually fails, any write or read on the fd would fail nonetheless, and we
  // wont's see spurious events.
  //
  // In case a separate connect() is called concurrently in another thread, and succeeds
  // immediately, any write or read on the fd would be attributed to the new connection, which would
  // have a new generation number. That is harmless, and only result into inflated generation
  // numbers.
  if (ret_val < 0 && ret_val != -EINPROGRESS) {
    return;
  }

  const struct connect_info_t* connect_info = active_connect_info_map.lookup(&id);
  if (connect_info == NULL) {
    return;
  }

  u32 tgid = id >> 32;
  submit_new_conn(ctx, tgid, (u32)connect_info->fd, connect_info->addr);
}

// This function stores the address to the sockaddr struct in the active_accept_info_map map.
// The key is the current pid/tgid.
//
// TODO(yzhao): We are not able to trace the source address/port yet. We might need to probe the
// socket() syscall.
static __inline void probe_entry_accept_impl(struct pt_regs* ctx, u64 id, int sockfd,
                                             struct sockaddr* addr, size_t* addrlen) {
  u32 tgid = id >> 32;

  if (!should_trace_tgid(tgid)) {
    return;
  }

  struct accept_info_t accept_info;
  memset(&accept_info, 0, sizeof(struct accept_info_t));
  accept_info.addr = addr;
  accept_info.addrlen = addrlen;
  active_accept_info_map.update(&id, &accept_info);
}

// Read the sockaddr values and write to the output buffer.
static __inline void probe_ret_accept_impl(struct pt_regs* ctx, u64 id) {
  int ret_fd = PT_REGS_RC(ctx);
  if (ret_fd < 0) {
    return;
  }

  struct accept_info_t* accept_info = active_accept_info_map.lookup(&id);
  if (accept_info == NULL) {
    return;
  }

  // Only record IP (IPV4 and IPV6) connections.
  if (!(accept_info->addr->sa_family == AF_INET || accept_info->addr->sa_family == AF_INET6)) {
    return;
  }

  u32 tgid = id >> 32;
  submit_new_conn(ctx, tgid, (u32)ret_fd, *((struct sockaddr_in6*)accept_info->addr));
}

static __inline void probe_entry_write_send(struct pt_regs* ctx, u64 id, int fd, char* buf,
                                            size_t count, const struct iovec* iov, size_t iovlen) {
  if (fd < 0) {
    return;
  }

  u32 tgid = id >> 32;

  if (!should_trace_tgid(tgid)) {
    return;
  }

  struct conn_info_t* conn_info = get_conn_info(tgid, fd);
  if (conn_info == NULL) {
    return;
  }

  // Note: From this point on, if exiting early, and conn_info->addr_valid == 0,
  // then call conn_info_map.delete(&tgid_fd) to avoid a BPF map leak.

  struct data_info_t write_info;
  memset(&write_info, 0, sizeof(struct data_info_t));
  write_info.timestamp_ns = bpf_ktime_get_ns();
  write_info.fd = fd;
  write_info.buf = buf;
  write_info.iov = iov;
  write_info.iovlen = iovlen;

  // TODO(yzhao): Split the interface such that the singular buf case and multiple bufs in msghdr
  // are handled separately without mixed interface. The plan is to factor out helper functions for
  // lower-level functionalities, and call them separately for each case.
  if (buf != NULL) {
    update_traffic_class(conn_info, kEgress, buf, count);
  } else if (write_info.iov != NULL && write_info.iovlen > 0) {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &write_info.iov[0]);
    update_traffic_class(conn_info, kEgress, iov_cpy.iov_base, iov_cpy.iov_len);
  }

  // Filter for request or response based on control flags and protocol type.
  if (!should_trace_conn(conn_info)) {
    u64 tgid_fd = gen_tgid_fd(tgid, fd);
    if (!conn_info->addr_valid) {
      conn_info_map.delete(&tgid_fd);
    }
    return;
  }

  active_write_info_map.update(&id, &write_info);
}

static __inline void probe_ret_write_send(struct pt_regs* ctx, u64 id) {
  ssize_t bytes_written = PT_REGS_RC(ctx);
  if (bytes_written <= 0) {
    // This write() call failed, or has nothing to write.
    return;
  }

  const struct data_info_t* write_info = active_write_info_map.lookup(&id);
  if (write_info == NULL) {
    return;
  }

  u32 tgid = id >> 32;
  u64 tgid_fd = gen_tgid_fd(tgid, write_info->fd);
  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info == NULL) {
    return;
  }

  struct socket_data_event_t* event = fill_event(kEgress, write_info, conn_info);
  if (event == NULL) {
    return;
  }

  // TODO(yzhao): Same TODO for split the interface.
  if (write_info->buf != NULL) {
    perf_submit_wrapper(ctx, kEgress, write_info->buf, bytes_written, conn_info, event);
  } else if (write_info->iov != NULL && write_info->iovlen > 0) {
    perf_submit_iovecs(ctx, kEgress, write_info->iov, write_info->iovlen, bytes_written, conn_info,
                       event);
  }
}

static __inline void probe_entry_read_recv(struct pt_regs* ctx, u64 id, int fd, char* buf,
                                           size_t count, const struct iovec* iov, size_t iovlen) {
  if (fd < 0) {
    return;
  }

  u32 tgid = id >> 32;

  if (!should_trace_tgid(tgid)) {
    return;
  }

  struct data_info_t read_info;
  memset(&read_info, 0, sizeof(struct data_info_t));
  read_info.timestamp_ns = bpf_ktime_get_ns();
  read_info.fd = fd;
  read_info.buf = buf;
  read_info.iov = iov;
  read_info.iovlen = iovlen;

  active_read_info_map.update(&id, &read_info);
}

static __inline void probe_ret_read_recv(struct pt_regs* ctx, u64 id) {
  ssize_t bytes_read = PT_REGS_RC(ctx);

  if (bytes_read <= 0) {
    // This read() call failed, or read nothing.
    return;
  }

  const struct data_info_t* read_info = active_read_info_map.lookup(&id);
  if (read_info == NULL) {
    return;
  }

  const char* buf = read_info->buf;

  u32 tgid = id >> 32;
  struct conn_info_t* conn_info = get_conn_info(tgid, read_info->fd);
  if (conn_info == NULL) {
    return;
  }

  // Note: From this point on, if exiting early, and conn_info->addr_valid == 0,
  // then call conn_info_map.delete(&tgid_fd) to avoid a BPF map leak.

  // TODO(yzhao): Same TODO for split the interface.
  if (buf != NULL) {
    update_traffic_class(conn_info, kIngress, buf, bytes_read);
  } else if (read_info->iov != NULL && read_info->iovlen > 0) {
    struct iovec iov_cpy;
    bpf_probe_read(&iov_cpy, sizeof(struct iovec), &read_info->iov[0]);
    // Ensure we are not reading beyond the available data.
    const size_t buf_size = iov_cpy.iov_len < bytes_read ? iov_cpy.iov_len : bytes_read;
    update_traffic_class(conn_info, kIngress, iov_cpy.iov_base, buf_size);
  }

  u64 tgid_fd = gen_tgid_fd(tgid, read_info->fd);

  // Filter for request or response based on control flags and protocol type.
  if (!should_trace_conn(conn_info)) {
    if (!conn_info->addr_valid) {
      conn_info_map.delete(&tgid_fd);
    }
    return;
  }

  struct socket_data_event_t* event = fill_event(kIngress, read_info, conn_info);
  if (event == NULL) {
    if (!conn_info->addr_valid) {
      conn_info_map.delete(&tgid_fd);
    }
    return;
  }

  // TODO(yzhao): Same TODO for split the interface.
  if (read_info->buf != NULL) {
    perf_submit_wrapper(ctx, kIngress, read_info->buf, bytes_read, conn_info, event);
  } else if (read_info->iov != NULL && read_info->iovlen > 0) {
    // TODO(yzhao): iov[0] is copied twice, once in calling update_traffic_class(), and here.
    // This happens to the write probes as well, but the calls are placed in the entry and return
    // probes respectively. Consider remove one copy.
    perf_submit_iovecs(ctx, kIngress, read_info->iov, read_info->iovlen, bytes_read, conn_info,
                       event);
  }
  return;
}

static __inline void probe_entry_close(struct pt_regs* ctx, u64 id, int fd) {
  if (fd < 0) {
    return;
  }

  u32 tgid = id >> 32;

  if (!should_trace_tgid(tgid)) {
    return;
  }

  u64 tgid_fd = gen_tgid_fd(tgid, fd);
  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info == NULL) {
    return;
  }

  struct close_info_t close_info;
  memset(&close_info, 0, sizeof(struct close_info_t));
  close_info.fd = fd;

  active_close_info_map.update(&id, &close_info);
}

static __inline void probe_ret_close(struct pt_regs* ctx, u64 id) {
  int ret_val = PT_REGS_RC(ctx);
  if (ret_val < 0) {
    // This close() call failed.
    return;
  }

  const struct close_info_t* close_info = active_close_info_map.lookup(&id);
  if (close_info == NULL) {
    return;
  }

  u32 tgid = id >> 32;
  u64 tgid_fd = gen_tgid_fd(tgid, close_info->fd);
  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info == NULL) {
    return;
  }

  // Only submit event to user-space if there was a corresponding open or data event reported.
  // This is to avoid polluting the perf buffer.
  if (conn_info->addr.sin6_family != 0 || conn_info->wr_seq_num != 0 ||
      conn_info->rd_seq_num != 0) {
    submit_close_event(ctx, conn_info);
  }

  conn_info_map.delete(&tgid_fd);
}

/***********************************************************
 * BPF syscall probe function entry-points
 ***********************************************************/

int syscall__probe_ret_open(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  int ret_fd = PT_REGS_RC(ctx);
  if (ret_fd > 0) {
    set_open_file(id, ret_fd);
  }
  return 0;
}

int syscall__probe_entry_connect(struct pt_regs* ctx, int sockfd, struct sockaddr* addr,
                                 size_t addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_connect_impl(ctx, id, sockfd, addr, addrlen);
  return 0;
}

int syscall__probe_ret_connect(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_connect_impl(ctx, id);
  active_connect_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_accept(struct pt_regs* ctx, int sockfd, struct sockaddr* addr,
                                size_t* addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_accept_impl(ctx, id, sockfd, addr, addrlen);
  return 0;
}

int syscall__probe_ret_accept(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_accept_impl(ctx, id);
  active_accept_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_accept4(struct pt_regs* ctx, int sockfd, struct sockaddr* addr,
                                 size_t* addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_accept_impl(ctx, id, sockfd, addr, addrlen);
  return 0;
}

int syscall__probe_ret_accept4(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_accept_impl(ctx, id);
  active_accept_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_write(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();

  if (is_open_file(id, fd)) {
    return 0;
  }

  probe_entry_write_send(ctx, id, fd, buf, count, /*iov*/ NULL, /*iovlen*/ 0);
  return 0;
}

int syscall__probe_ret_write(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_write_send(ctx, id);
  active_write_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_send(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_write_send(ctx, id, fd, buf, count, /*iov*/ NULL, /*iovlen*/ 0);
  return 0;
}

int syscall__probe_ret_send(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_write_send(ctx, id);
  active_write_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_read(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();

  if (is_open_file(id, fd)) {
    return 0;
  }

  probe_entry_read_recv(ctx, id, fd, buf, count, /*iov*/ NULL, /*iovlen*/ 0);
  return 0;
}

int syscall__probe_ret_read(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_read_recv(ctx, id);
  active_read_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_recv(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_read_recv(ctx, id, fd, buf, count, /*iov*/ NULL, /*iovlen*/ 0);
  return 0;
}

int syscall__probe_ret_recv(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_read_recv(ctx, id);
  active_read_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_sendto(struct pt_regs* ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();
  probe_entry_write_send(ctx, id, fd, buf, count, /*iov*/ NULL, /*iovlen*/ 0);
  return 0;
}

int syscall__probe_ret_sendto(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_write_send(ctx, id);
  active_write_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_sendmsg(struct pt_regs* ctx, int fd, const struct user_msghdr* msghdr) {
  u64 id = bpf_get_current_pid_tgid();

  if (msghdr != NULL) {
    probe_entry_write_send(ctx, id, fd, /*buf*/ NULL, /*count*/ 0, msghdr->msg_iov,
                           msghdr->msg_iovlen);
  }
  return 0;
}

int syscall__probe_ret_sendmsg(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_write_send(ctx, id);
  active_write_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_recvmsg(struct pt_regs* ctx, int fd, struct user_msghdr* msghdr) {
  u64 id = bpf_get_current_pid_tgid();
  if (msghdr != NULL) {
    probe_entry_read_recv(ctx, id, fd, /*buf*/ NULL, /*count*/ 0, msghdr->msg_iov,
                          msghdr->msg_iovlen);
  }
  return 0;
}

int syscall__probe_ret_recvmsg(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_read_recv(ctx, id);
  active_read_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_writev(struct pt_regs* ctx, int fd, const struct iovec* iov, int iovlen) {
  if (iov == NULL && iovlen <= 0) {
    return 0;
  }

  u64 id = bpf_get_current_pid_tgid();

  if (is_open_file(id, fd)) {
    return 0;
  }

  probe_entry_write_send(ctx, id, fd, /*buf*/ NULL, /*count*/ 0, iov, iovlen);
  return 0;
}

int syscall__probe_ret_writev(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_write_send(ctx, id);
  active_write_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_readv(struct pt_regs* ctx, int fd, struct iovec* iov, int iovlen) {
  if (iov == NULL && iovlen <= 0) {
    return 0;
  }

  u64 id = bpf_get_current_pid_tgid();
  if (is_open_file(id, fd)) {
    return 0;
  }

  probe_entry_read_recv(ctx, id, fd, /*buf*/ NULL, /*count*/ 0, iov, iovlen);
  return 0;
}

int syscall__probe_ret_readv(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_read_recv(ctx, id);
  active_read_info_map.delete(&id);
  return 0;
}

int syscall__probe_entry_close(struct pt_regs* ctx, unsigned int fd) {
  u64 id = bpf_get_current_pid_tgid();
  clear_open_file(id, fd);
  probe_entry_close(ctx, id, fd);
  return 0;
}

int syscall__probe_ret_close(struct pt_regs* ctx) {
  u64 id = bpf_get_current_pid_tgid();
  probe_ret_close(ctx, id);
  active_close_info_map.delete(&id);
  return 0;
}

// TODO(oazizi): Look into the following opens:
// 1) Why does the syscall table only include sendto, while Linux source code and man page list both
// sendto and send? 2) What do we do when the sendto() is called with a dest_addr provided? I
// believe this overrides the conn_info.

// Includes HTTP2 tracing probes.
#include "src/stirling/bcc_bpf/go_grpc.c"
