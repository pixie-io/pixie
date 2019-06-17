#include <uapi/linux/ptrace.h>
#include <uapi/linux/in6.h>
#include <linux/socket.h>
#include "src/stirling/bcc_bpf/socket_trace.h"

// This is the perf buffer for BPF program to export data from kernel to user space.
BPF_PERF_OUTPUT(socket_http_resp_events);
BPF_PERF_OUTPUT(socket_mysql_events);
BPF_PERF_OUTPUT(socket_http2_events);

/***********************************************************
 * Internal structs and definitions
 ***********************************************************/

struct addr_info_t {
  struct sockaddr *addr;
  size_t *addrlen;
} __attribute__((__packed__, aligned(8)));

struct data_info_t {
  u64 lookup_fd;
  const char *buf;
} __attribute__((__packed__, aligned(8)));


// Map from user-space file descriptors to the connections obtained from accept() syscall.
// Tracks connection from accept() -> close().
// Key is {tgid, fd}.
BPF_HASH(conn_info_map, u64, struct conn_info_t);

// Map from threads to its ongoing accept() syscall's input argument.
// Tracks accept() call from entry -> exit.
// Key is {tgid, pid}.
BPF_HASH(active_sock_addr, u64, struct addr_info_t);

// Map from threads to its ongoing write() syscall's input argument.
// Tracks write() call from entry -> exit.
// Key is {tgid, pid}.
//
// TODO(yzhao): Consider merging this with active_sock_addr.
BPF_HASH(active_write_info_map, u64, struct data_info_t);
BPF_HASH(active_read_info_map, u64, struct data_info_t);

// TODO(yzhao): Change to use tgid+fd as the key.
// Map from process to the next available connection ID.
// Key is tgid
BPF_HASH(proc_conn_map, u32, u32);

static inline __attribute__((__always_inline__))
uint32_t get_conn_id(u32 tgid) {
  u32 conn_id = 0;
  u32* curr_conn_id = proc_conn_map.lookup_or_init(&tgid, &conn_id);
  if (curr_conn_id != NULL) {
    conn_id = (*curr_conn_id)++;
  }
  return conn_id;
}

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(data_buffer_heap, struct socket_data_event_t, 1);

// Attribute copied from:
// https://github.com/iovisor/bcc/blob/ef9d83f289222df78f0b16a04ade7c393bf4d83d/\
// examples/cpp/pyperf/PyPerfBPFProgram.cc#L168
static inline __attribute__((__always_inline__))
struct socket_data_event_t* data_buffer() {
  u32 kZero = 0;
  return data_buffer_heap.lookup(&kZero);
}

/***********************************************************
 * Buffer processing helper functions
 ***********************************************************/

static bool is_http_response(const char *buf, size_t count) {
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

static bool is_http_request(const char *buf, size_t count) {
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

static bool is_mysql_protocol(const char *buf, size_t count) {
  if (count < 1){
    return false;
  }

  // MySQL queries appear to start with this special SYN character (0x16).
  // This was discovered experimentally, and is not guaranteed to be robust.
  // TODO(oazizi): Find a better way.
  // 0x16 represents COM_STMT_PREPARE in the MySQL protocol.
  if (*buf == 0x16) {
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
static bool is_http2_connection_preface(const char *buf, size_t count) {
  if (count < 3) {
    return false;
  }
  return buf[0] == 'P' && buf[1] == 'R' && buf[2] == 'I';
}

static u32 infer_protocol(const char *buf, size_t count) {
  return is_http_response(buf, count)  ? kProtocolHTTPResponse :
         is_http_request(buf, count)   ? kProtocolHTTPRequest :
         is_mysql_protocol(buf, count) ? kProtocolMySQL :
         is_http2_connection_preface(buf, count) ? kProtocolHTTP2 :
                                         kProtocolUnknown;
}

static struct conn_info_t* get_conn_info(u64 lookup_fd, const char* buf, size_t count) {
  struct conn_info_t* conn_info = conn_info_map.lookup(&lookup_fd);

  // Try to infer connection type (protocol) based on data.
  // If protocol is detected, then let it through, even though accept()/connect() was not captured.
  // Won't know remote endpoint (remote IP and port), but still let it through.
  // TODO(oazizi): Future architecture should have user-land provide this information.
  if (conn_info == NULL) {
    u32 protocol = infer_protocol(buf, count);
    if (protocol != kProtocolUnknown) {
      struct conn_info_t new_conn_info;
      memset(&new_conn_info, 0, sizeof(struct conn_info_t));
      new_conn_info.protocol = protocol;
      conn_info = conn_info_map.lookup_or_init(&lookup_fd, &new_conn_info);
    }
  }

  // Attempt to classify protocol, if unknown.
  if (conn_info != NULL && conn_info->protocol == kProtocolUnknown) {
    // TODO(oazizi): Look for only certain protocols on write/send()?
    conn_info->protocol = infer_protocol(buf, count);
  }

  return conn_info;
}

/***********************************************************
 * BPF syscall probe functions
 ***********************************************************/

// This function stores the address to the sockaddr struct in the active_sock_addr map.
// The key is the current pid/tgid.
//
// TODO(yzhao): We are not able to trace the source address/port yet. We might need to probe the
// socket() syscall.
static int probe_entry_accept_impl(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  struct addr_info_t addr_info;

  addr_info.addr = addr;
  addr_info.addrlen = addrlen;
  active_sock_addr.update(&id, &addr_info);

  return 0;
}

// Read the sockaddr values and write to the output buffer.
static int probe_ret_accept_impl(struct pt_regs *ctx) {
  u64 id = bpf_get_current_pid_tgid();
  u32 tgid = id >> 32;

  int ret_fd = PT_REGS_RC(ctx);
  if (ret_fd < 0) {
    goto done;
  }

  struct addr_info_t *addr_info = active_sock_addr.lookup(&id);
  if (addr_info == NULL) {
    goto done;
  }

  // Only record IP (IPV4 and IPV6) connections.
  if (!(addr_info->addr->sa_family == AF_INET || addr_info->addr->sa_family == AF_INET6)) {
    goto done;
  }

  // Prepend TGID to make the FD unique across processes.
  u64 tgid_fd = ((u64)tgid << 32) | (u32)ret_fd;

  u32 conn_id = get_conn_id(tgid);

  struct conn_info_t conn_info;
  memset(&conn_info, 0, sizeof(struct conn_info_t));
  conn_info.timestamp_ns = bpf_ktime_get_ns();
  conn_info.addr = *((struct sockaddr_in6*) addr_info->addr);
  conn_info.conn_id = conn_id;
  conn_info.protocol = kProtocolUnknown;
  conn_info.wr_seq_num = 0;
  conn_info.rd_seq_num = 0;
  conn_info_map.update(&tgid_fd, &conn_info);

 done:
  // Regardless of what happened, after accept() returns, there is no need to track the sock_addr
  // being accepted during the accept() call, therefore we can remove the entry.
  active_sock_addr.delete(&id);
  return 0;
}

static int probe_entry_write_send(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  if (fd < 0) { return 0; }

  u64 id = bpf_get_current_pid_tgid();
  u32 tgid = id >> 32;
  u64 lookup_fd = ((u64)tgid << 32) | (u32)fd;

  struct conn_info_t* conn_info = get_conn_info(lookup_fd, buf, count);
  if (conn_info == NULL) {
    return 0;
  }

  // If this connection has an unknown protocol, abort (to avoid pollution).
  if (conn_info->protocol == kProtocolUnknown) {
    return 0;
  }

  struct data_info_t write_info;
  memset(&write_info, 0, sizeof(struct data_info_t));
  write_info.lookup_fd = lookup_fd;
  write_info.buf = buf;

  active_write_info_map.update(&id, &write_info);
  return 0;
}

static int probe_ret_write_send(struct pt_regs *ctx, uint32_t event_type) {
  u64 id = bpf_get_current_pid_tgid();

  int32_t bytes_written = PT_REGS_RC(ctx);
  if (bytes_written <= 0) {
    // This write() call failed, or has nothing to write.
    goto done;
  }

  const struct data_info_t* write_info = active_write_info_map.lookup(&id);
  if (write_info == NULL) {
    goto done;
  }

  uint64_t lookup_fd = write_info->lookup_fd;
  struct conn_info_t* conn_info = conn_info_map.lookup(&lookup_fd);
  if (conn_info == NULL) {
    goto done;
  }

  struct socket_data_event_t* event = data_buffer();
  if (event == NULL) {
    goto done;
  }

  event->attr.conn_info = *conn_info;
  // Increment sequence number after copying so the index is 0-based.
  event->attr.seq_num = conn_info->wr_seq_num;
  ++conn_info->wr_seq_num;
  event->attr.event_type = event_type;
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.tgid = id >> 32;
  event->attr.fd = (uint32_t)write_info->lookup_fd;

  const uint32_t buf_size = bytes_written < sizeof(event->msg) ? bytes_written : sizeof(event->msg);
  event->attr.msg_size = buf_size;
  bpf_probe_read(&event->msg, buf_size, write_info->buf);

  // Write snooped arguments to perf ring buffer.
  const uint32_t size_to_submit = sizeof(event->attr) + buf_size;
  switch (conn_info->protocol) {
    case kProtocolHTTPResponse: socket_http_resp_events.perf_submit(ctx, event, size_to_submit); break;
    case kProtocolMySQL: socket_mysql_events.perf_submit(ctx, event, size_to_submit); break;
    case kProtocolHTTP2: socket_http2_events.perf_submit(ctx, event, size_to_submit); break;
  }

 done:
  // Regardless of what happened, after write/send/sendto() returns, there is no need to track the fd & buf
  // being accepted by write/send/sendto() call, therefore we can remove the entry.
  active_write_info_map.delete(&id);
  return 0;
}

static int probe_entry_read_recv(struct pt_regs *ctx, unsigned int fd, char* buf, size_t count, uint32_t event_type) {
  u64 id = bpf_get_current_pid_tgid();
  u32 tgid = id >> 32;
  u64 lookup_fd = ((u64)tgid << 32) | (u32)fd;

  struct data_info_t read_info;
  memset(&read_info, 0, sizeof(struct data_info_t));
  read_info.lookup_fd = lookup_fd;
  read_info.buf = buf;

  active_read_info_map.update(&id, &read_info);
  return 0;
}

static int probe_ret_read_recv(struct pt_regs *ctx, uint32_t event_type) {
  u64 id = bpf_get_current_pid_tgid();

  int32_t bytes_read = PT_REGS_RC(ctx);

  if (bytes_read <= 0) {
    // This read() call failed, or read nothing.
    goto done;
  }

  const struct data_info_t* read_info = active_read_info_map.lookup(&id);
  if (read_info == NULL) {
    goto done;
  }

  u64 lookup_fd = read_info->lookup_fd;

  const char* buf = read_info->buf;

  struct conn_info_t* conn_info = get_conn_info(lookup_fd, buf, bytes_read);
  if (conn_info == NULL) {
    goto done;
  }

  // If this connection has an unknown protocol, abort (to avoid pollution).
  if (conn_info->protocol == kProtocolUnknown) {
    return 0;
  }

  struct socket_data_event_t* event = data_buffer();
  if (event == NULL) {
    goto done;
  }

  event->attr.conn_info = *conn_info;
  event->attr.seq_num = conn_info->rd_seq_num;
  ++conn_info->rd_seq_num;
  event->attr.event_type = event_type;
  event->attr.timestamp_ns = bpf_ktime_get_ns();
  event->attr.tgid = id >> 32;
  event->attr.fd = (uint32_t)read_info->lookup_fd;

  const uint32_t buf_size = bytes_read < sizeof(event->msg) ? bytes_read : sizeof(event->msg);
  event->attr.msg_size = buf_size;
  bpf_probe_read(&event->msg, buf_size, buf);

  // Write snooped arguments to perf ring buffer. Note that msg field is truncated.
  const uint32_t size_to_submit = sizeof(event->attr) + buf_size;
  switch (conn_info->protocol) {
    case kProtocolHTTPResponse: socket_http_resp_events.perf_submit(ctx, event, size_to_submit); break;
    case kProtocolMySQL: socket_mysql_events.perf_submit(ctx, event, size_to_submit); break;
    case kProtocolHTTP2: socket_http2_events.perf_submit(ctx, event, size_to_submit); break;
  }

  done:
  // Regardless of what happened, after write/send() returns, there is no need to track the fd & buf
  // being accepted by write/send() call, therefore we can remove the entry.
  active_read_info_map.delete(&id);
  return 0;
}

int probe_close(struct pt_regs *ctx, int fd) {
  if (fd < 0) { return 0; }
  u64 id = bpf_get_current_pid_tgid();
  u32 tgid = id >> 32;
  u64 lookup_fd = ((u64)tgid << 32) | fd;
  conn_info_map.delete(&lookup_fd);
  return 0;
}

/***********************************************************
 * BPF syscall probe function entry-points
 ***********************************************************/

int probe_entry_accept(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  return probe_entry_accept_impl(ctx, sockfd, addr, addrlen);
}

int probe_ret_accept(struct pt_regs *ctx) {
  return probe_ret_accept_impl(ctx);
}

int probe_entry_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  return probe_entry_accept_impl(ctx, sockfd, addr, addrlen);
}

int probe_ret_accept4(struct pt_regs *ctx) {
  return probe_ret_accept_impl(ctx);
}

int probe_entry_write(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_write_send(ctx, fd, buf, count);
}

int probe_ret_write(struct pt_regs *ctx) {
  return probe_ret_write_send(ctx, kEventTypeSyscallWriteEvent);
}

int probe_entry_send(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_write_send(ctx, fd, buf, count);
}

int probe_ret_send(struct pt_regs *ctx) {
  return probe_ret_write_send(ctx, kEventTypeSyscallSendEvent);
}

int probe_entry_read(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_read_recv(ctx, fd, buf, count, kEventTypeSyscallReadEvent);
}

int probe_ret_read(struct pt_regs *ctx) {
  return probe_ret_read_recv(ctx, kEventTypeSyscallReadEvent);
}

int probe_entry_recv(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_read_recv(ctx, fd, buf, count, kEventTypeSyscallRecvEvent);
}

int probe_ret_recv(struct pt_regs *ctx) {
  return probe_ret_read_recv(ctx, kEventTypeSyscallRecvEvent);
}

int probe_entry_sendto(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_write_send(ctx, fd, buf, count);
}

int probe_ret_sendto(struct pt_regs *ctx) {
  return probe_ret_write_send(ctx, kEventTypeSyscallSendEvent);
}

// TODO(oazizi): Look into the following opens:
// 1) Should we trace sendmsg(), which is another syscall, but with a different interface?
// 2) Why does the syscall table only include sendto, while Linux source code and man page list both sendto and send?
// 3) What do we do when the sendto() is called with a dest_addr provided? I believe this overrides the conn_info.
