#include <linux/errno.h>
#include <linux/in6.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/socket.h>

#include "src/stirling/bcc_bpf_interface/grpc.h"
#include "src/stirling/bcc_bpf_interface/http2.h"
#include "src/stirling/bcc_bpf_interface/log_event.h"
#include "src/stirling/bcc_bpf_interface/socket_trace.h"

#include "src/stirling/bcc_bpf/logging.h"
#include "src/stirling/bcc_bpf/utils.h"

// This is how Linux converts nanoseconds to clock ticks.
// Used to report PID start times in clock ticks, just like /proc/<pid>/stat does.
static __inline u64 pl_nsec_to_clock_t(u64 x) { return div_u64(x, NSEC_PER_SEC / USER_HZ); }

// TODO(yzhao): Investigate the performance overhead of active_*_info_map.delete(id), when id is not
// in the map. If it's significant, change to only call delete() after knowing that id is in the
// map.

// This keeps instruction count below BPF's limit of 4096 per probe.
// TODO(yzhao): Investigate using tail call to reuse stack space to support loop.
// TODO(yzhao): Optimize the code to remove unneeded code, and increase the loop count.
// TODO(PL-914): 4.13 and older kernel versions need smaller number, 10 is tested to work.
// See the referenced Jira issue for more details.
#define LOOP_LIMIT 35

// This is the perf buffer for BPF program to export data from kernel to user space.
BPF_PERF_OUTPUT(socket_http_events);
BPF_PERF_OUTPUT(socket_mysql_events);
BPF_PERF_OUTPUT(socket_open_conns);
BPF_PERF_OUTPUT(socket_close_conns);

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
// The bits are defined in ReqRespRole enum, kRoleRequestor or kRoleResponder. kRoleUnknown is not
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

// Map from TGID, FD pair to a unique identifier (generation) of that pair.
// Key is {tgid, fd}.
BPF_HASH(proc_conn_map, u64, u32);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(data_buffer_heap, struct socket_data_event_t, 1);

// This array records singular values that are used by probes. We group them together to reduce the
// number of arrays with only 1 element.
BPF_PERCPU_ARRAY(control_values, s64, kNumControlValues);

/***********************************************************
 * General helper functions
 ***********************************************************/

static __inline uint64_t get_tgid_start_time() {
  struct task_struct* task = (struct task_struct*)bpf_get_current_task();
  return pl_nsec_to_clock_t(task->group_leader->start_time);
}

static __inline uint32_t get_tgid_fd_generation(u64 tgid_fd) {
  u32 init_tgid_fd_generation = 0;
  u32* tgid_fd_generation = proc_conn_map.lookup_or_init(&tgid_fd, &init_tgid_fd_generation);
  return (*tgid_fd_generation)++;
}

static __inline void set_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;
  bool kTrue = 1;
  open_file_map.insert(&tgid_fd, &kTrue);
}

static __inline bool is_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;
  bool* open_file = open_file_map.lookup(&tgid_fd);
  return (open_file != NULL);
}

static __inline void clear_open_file(u64 id, int fd) {
  u32 tgid = id >> 32;
  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;
  open_file_map.delete(&tgid_fd);
}

static __inline struct conn_info_t* get_conn_info(u32 tgid, u32 fd) {
  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;
  struct conn_info_t new_conn_info;
  memset(&new_conn_info, 0, sizeof(struct conn_info_t));
  struct conn_info_t* conn_info = conn_info_map.lookup_or_init(&tgid_fd, &new_conn_info);
  // Use TGID zero to detect that a new conn_info was initialized.
  if (conn_info->conn_id.tgid == 0) {
    // If lookup_or_init initialized a new conn_info, we need to set some fields.
    conn_info->conn_id.tgid = tgid;
    conn_info->conn_id.tgid_start_time_ticks = get_tgid_start_time();
    conn_info->conn_id.fd = fd;
    // Note that many calls to this function are not for socket descriptors,
    // so we are actually "wasting" generations numbers.
    // But this is still the most straightforward thing to do, and
    // using one generation number per second should still provide 136 years of coverage.
    conn_info->conn_id.generation = get_tgid_fd_generation(tgid_fd);
  }
  return conn_info;
}

/***********************************************************
 * Trace filtering functions
 ***********************************************************/

static __inline bool should_trace(const struct traffic_class_t* traffic_class) {
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

static __inline struct socket_data_event_t* fill_event(TrafficDirection direction,
                                                       const struct conn_info_t* conn_info) {
  u32 kZero = 0;
  struct socket_data_event_t* event = data_buffer_heap.lookup(&kZero);
  if (event == NULL) {
    return NULL;
  }
  event->attr.direction = direction;
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.conn_id.tgid = conn_info->conn_id.tgid;
  event->attr.conn_id.tgid_start_time_ticks = conn_info->conn_id.tgid_start_time_ticks;
  event->attr.conn_id.fd = conn_info->conn_id.fd;
  event->attr.conn_id.generation = conn_info->conn_id.generation;
  event->attr.traffic_class = conn_info->traffic_class;
  return event;
}

/***********************************************************
 * Traffic inference helper functions
 ***********************************************************/

static __inline bool is_http_response(const char* buf, size_t count) {
  // Smallest HTTP response is 17 characters:
  // HTTP/1.1 200 OK\r\n
  // Use 16 here to be conservative.
  if (count < 16) {
    return false;
  }

  if (buf[0] == 'H' && buf[1] == 'T' && buf[2] == 'T' && buf[3] == 'P') {
    return true;
  }

  return false;
}

static __inline bool is_http_request(const char* buf, size_t count) {
  // Smallest HTTP response is 16 characters:
  // GET x HTTP/1.1\r\n
  if (count < 16) {
    return false;
  }

  if (buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
    return true;
  }
  if (buf[0] == 'P' && buf[1] == 'O' && buf[2] == 'S' && buf[3] == 'T') {
    return true;
  }
  // TODO(oazizi): Should we add PUT, DELETE, HEAD, and perhaps others?

  return false;
}

// TODO(oazizi/yzhao): This function only check two bytes, buf[3] == 0 and buf[4], which is too
// error-prone.  Add stronger protocol detection.
static __inline bool is_mysql_protocol(const char* buf, size_t count) {
  // MySQL packets start with a 3-byte packet length and a 1-byte packet number.
  // The 5th byte on a request contains a command that tells the type.
  // 0x16: Statement Prepare, 0x17 Statement Execute, 0x19 Statement Close,
  // 0x03 Query.
  // TODO(chengruizhe): Add more commands when more are supported.
  if (count < 5) {
    return false;
  }

  // The packet number of a request should always be 0.
  if (buf[3] != 0) {
    return false;
  }

  // Assuming that the lengh of a request is less than 2^16 to avoid false
  // positive flagging as MySQL, which statiscally happen for a single-byte
  // check.
  if (buf[2] != 0) {
    return false;
  }

  if (buf[4] == kStmtPreparePrefix || buf[4] == kStmtExecutePrefix || buf[4] == kStmtClosePrefix ||
      buf[4] == kQueryPrefix) {
    return true;
  }
  return false;
}

// Technically, HTTP2 client connection preface is 24 octes [1]. Practically,
// the first 3 shall be sufficient. Note this is sent from client,
// so it would be captured on server's read()/recvfrom()/recvmsg() or client's
// write()/sendto()/sendmsg().
//
// [1] https://http2.github.io/http2-spec/#ConnectionHeader
static __inline bool is_http2_connection_preface(const char* buf, size_t count) {
  if (count < 3) {
    return false;
  }
  return buf[0] == 'P' && buf[1] == 'R' && buf[2] == 'I';
}

static __inline struct traffic_class_t infer_traffic(TrafficDirection direction, const char* buf,
                                                     size_t count) {
  struct traffic_class_t traffic_class;
  traffic_class.protocol = kProtocolUnknown;
  traffic_class.role = kRoleUnknown;

  if (is_http_response(buf, count)) {
    traffic_class.protocol = kProtocolHTTP;
    traffic_class.role = direction == kEgress ? kRoleResponder : kRoleRequestor;
  } else if (is_http_request(buf, count)) {
    traffic_class.protocol = kProtocolHTTP;
    traffic_class.role = direction == kEgress ? kRoleRequestor : kRoleResponder;
  } else if (is_mysql_protocol(buf, count)) {
    traffic_class.protocol = kProtocolMySQL;
    traffic_class.role = direction == kEgress ? kRoleRequestor : kRoleResponder;
  } else if (is_http2_connection_preface(buf, count)) {
    traffic_class.protocol = kProtocolHTTP2;
    traffic_class.role = direction == kEgress ? kRoleRequestor : kRoleResponder;
  } else if (looks_like_grpc_req_http2_headers_frame(buf, count)) {
    // Combining this with the above else if branch causes bpf loading failure.
    // TODO(yzhao): Figure out why.
    traffic_class.protocol = kProtocolHTTP2;
    traffic_class.role = direction == kEgress ? kRoleRequestor : kRoleResponder;
  }
  return traffic_class;
}

// TODO(oazizi): This function should go away once the protocol is identified externally.
//               Also, could move this function into the header file, so we can test it.
static __inline void update_traffic_class(struct conn_info_t* conn_info, TrafficDirection direction,
                                          const char* buf, size_t count) {
  // TODO(oazizi): Future architecture should have user-land provide the traffic_class.
  // TODO(oazizi): conn_info currently works only if tracing on the send or recv side of a process,
  //               but not both simultaneously, because we need to mark two traffic classes.

  // Try to infer connection type (protocol) based on data.
  // If protocol is detected, then let it through, even though accept()/connect() was not captured.
  if (conn_info != NULL && conn_info->traffic_class.protocol == kProtocolUnknown) {
    // TODO(oazizi): Look for only certain protocols on write/send()?
    struct traffic_class_t traffic_class = infer_traffic(direction, buf, count);
    conn_info->traffic_class = traffic_class;
  }
}

/***********************************************************
 * Perf submit functions
 ***********************************************************/

static __inline void submit_new_conn(struct pt_regs* ctx, u32 tgid, u32 fd,
                                     struct sockaddr_in6 addr) {
  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;

  struct conn_info_t conn_info;
  memset(&conn_info, 0, sizeof(struct conn_info_t));
  conn_info.timestamp_ns = bpf_ktime_get_ns();
  conn_info.addr = addr;
  conn_info.traffic_class.protocol = kProtocolUnknown;
  conn_info.traffic_class.role = kRoleUnknown;
  conn_info.wr_seq_num = 0;
  conn_info.rd_seq_num = 0;
  conn_info.conn_id.tgid = tgid;
  conn_info.conn_id.tgid_start_time_ticks = get_tgid_start_time();
  conn_info.conn_id.fd = fd;
  conn_info.conn_id.generation = get_tgid_fd_generation(tgid_fd);

  conn_info_map.update(&tgid_fd, &conn_info);
  socket_open_conns.perf_submit(ctx, &conn_info, sizeof(struct conn_info_t));
}

static __inline void submit_close_event(struct pt_regs* ctx, struct conn_info_t* conn_info) {
  // Update timestamp to reflect the close event.
  conn_info->timestamp_ns = bpf_ktime_get_ns();
  socket_close_conns.perf_submit(ctx, conn_info, sizeof(struct conn_info_t));
}

// TODO(yzhao): We can write a test for this, by define a dummy bpf_probe_read() function. Similar
// in idea to a mock in normal C++ code.
//
// Writes the input buf to event, and submits the event to the corresponding perf buffer.
//
// Returns the bytes output from the input buf. Note that is not the total bytes submitted to the
// perf buffer, which includes additional metadata.
static __inline size_t perf_submit_buf(struct pt_regs* ctx, const TrafficDirection direction,
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
  //
  // But this does not work in kernel versions 4.14 or older, because the verifier does not like
  // a bpf_probe_read with size 0, and it can't prove that the size won't be zero. This
  // is true even if we include a 'if (msg_size > 0)' around the code.
  //
  // Tested to work on GKE node with 4.14.127+ kernel.
  //
  // Useful link: https://www.mail-archive.com/netdev@vger.kernel.org/msg199918.html

  size_t msg_submit_size = 0;
  if (buf_size >= sizeof(event->msg)) {
    msg_submit_size = sizeof(event->msg);
    bpf_probe_read(&event->msg, msg_submit_size, buf);
  } else if (buf_size > 0) {
    msg_submit_size = buf_size;
    // Read one extra byte, because older kernels (4.14) don't accept 0 as the second argument,
    // And their verifier can't prove that it won't be zero (despite the obvious if-statement).
    bpf_probe_read(&event->msg, msg_submit_size + 1, buf);
  }

  event->attr.msg_size = buf_size;

  // Write snooped arguments to perf ring buffer.
  const size_t size_to_submit = sizeof(event->attr) + msg_submit_size;
  switch (event->attr.traffic_class.protocol) {
    case kProtocolHTTP:
    case kProtocolHTTP2:
      socket_http_events.perf_submit(ctx, event, size_to_submit);
      break;
    case kProtocolMySQL:
      socket_mysql_events.perf_submit(ctx, event, size_to_submit);
      break;
    default:
      break;
  }
  return msg_submit_size;
}

static __inline void perf_submit_wrapper(struct pt_regs* ctx, const TrafficDirection direction,
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
      // The modulo is essentially a NOP, but informs the BPF verifier about the safety bounds of
      // bytes_remaining. Note that this only works if MAX_MSG_SIZE is a power-of-2. Otherwise clang
      // will produce different code that will not keep the verifier happy.
      size_t bytes_to_submit = bytes_remaining % MAX_MSG_SIZE;
      perf_submit_buf(ctx, direction, buf + i * MAX_MSG_SIZE, bytes_to_submit, conn_info, event);
      bytes_remaining = bytes_remaining - bytes_to_submit;
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

static __inline void perf_submit_iovecs(struct pt_regs* ctx, const TrafficDirection direction,
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

  struct data_info_t write_info;
  memset(&write_info, 0, sizeof(struct data_info_t));
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
  if (!should_trace(&conn_info->traffic_class)) {
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

  u64 tgid_fd = ((id >> 32) << 32) | write_info->fd;
  struct conn_info_t* conn_info = conn_info_map.lookup(&tgid_fd);
  if (conn_info == NULL) {
    return;
  }

  struct socket_data_event_t* event = fill_event(kEgress, conn_info);
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

  // Filter for request or response based on control flags and protocol type.
  if (!should_trace(&conn_info->traffic_class)) {
    return;
  }

  struct socket_data_event_t* event = fill_event(kIngress, conn_info);
  if (event == NULL) {
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

  u64 tgid_fd = ((u64)tgid << 32) | (u32)fd;
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

  u64 tgid_fd = ((id >> 32) << 32) | close_info->fd;
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
