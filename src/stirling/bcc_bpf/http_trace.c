#include <uapi/linux/ptrace.h>
#include <uapi/linux/in6.h>
#include <linux/socket.h>

// This is copied from http_trace.h, with comments removed, so that this whole file can be
// hermetically installed as a BPF program.
//
// TODO(PL-451): The struct definitions that are reused in HTTPTraceConnector should be shared from
// the same header file.

struct accept_info_t {
  uint64_t timestamp_ns;
  struct sockaddr_in6 addr;
  uint32_t conn_id;
  uint64_t seq_num;
} __attribute__((__packed__, aligned(8)));

#define MAX_MSG_SIZE 4096
struct syscall_write_event_t {
  struct attr_t {
    struct accept_info_t accept_info;
    uint64_t time_stamp_ns;
    uint32_t tgid;
    uint32_t pid;
    uint32_t fd;
    uint32_t event_type;
    uint32_t msg_bytes;
    uint32_t msg_buf_size;
  } attr;
  char msg[MAX_MSG_SIZE];
} __attribute__((__packed__));
const uint32_t kEventTypeSyscallWriteEvent = 1;
const uint32_t kEventTypeSyscallSendEvent = 2;

// This is the perf buffer for BPF program to export data from kernel to user space.
BPF_PERF_OUTPUT(syscall_write_events);

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(write_buffer_heap, struct syscall_write_event_t, 1);

/***********************************************************
 * BPF Program that traces a request lifecycle:
 *   Starts at accept4 (establish connection)
 *   Write (extract data)
 *   Close (complete request)
 **********************************************************/
struct addr_info_t {
  struct sockaddr *addr;
  size_t *addrlen;
};

// Map from threads to its ongoing accept() syscall's input argument.
// Key is {tgid, pid}.
BPF_HASH(active_sock_addr, u64, struct addr_info_t);

// Map from user-space file descriptors to the connections obtained from accept() syscall.
// Key is {tgid, fd}.
BPF_HASH(accept_info_map, u64, struct accept_info_t);

struct write_info_t {
  u64 lookup_fd;
  char* buf;
} __attribute__((__packed__, aligned(8)));

// Map from threads to its ongoing write() syscall's input argument.
// Key is {tgid, pid}.
//
// TODO(yzhao): Consider merging this with active_sock_addr.
BPF_HASH(active_write_info_map, u64, struct write_info_t);

// Map from process to the next available connection ID.
// Key is tgid
BPF_HASH(proc_conn_map, u32, u32);

// This function stores the address to the sockaddr struct in the active_sock_addr map.
// The key is the current pid/tgid.
//
// TODO(yzhao): We are not able to trace the source address/port yet. We might need to probe the
// socket() syscall.
int probe_entry_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  struct addr_info_t addr_info;

  addr_info.addr = addr;
  addr_info.addrlen = addrlen;
  active_sock_addr.update(&id, &addr_info);

  return 0;
}

// Read the sockaddr values and write to the output buffer.
int probe_ret_accept4(struct pt_regs *ctx) {
  bool TRUE = true;

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

  u32 kZero = 0;
  u32 conn_id = 0;
  u32* curr_conn_id = proc_conn_map.lookup_or_init(&tgid, &kZero);
  if (curr_conn_id != NULL) {
    conn_id = (*curr_conn_id)++;
  }

  struct accept_info_t accept_info;
  memset(&accept_info, 0, sizeof(struct accept_info_t));
  accept_info.timestamp_ns = bpf_ktime_get_ns();
  accept_info.addr = *((struct sockaddr_in6*) addr_info->addr);
  accept_info.conn_id = conn_id;
  accept_info.seq_num = 0;
  accept_info_map.update(&tgid_fd, &accept_info);

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

  struct accept_info_t *accept_info = accept_info_map.lookup(&lookup_fd);
  if (accept_info == NULL) {
    // Bail early if we aren't tracking fd.
    return 0;
  }

  struct write_info_t write_info;
  memset(&write_info, 0, sizeof(struct write_info_t));
  write_info.lookup_fd = lookup_fd;
  write_info.buf = buf;

  active_write_info_map.update(&id, &write_info);
  return 0;
}

static int probe_ret_write_send(struct pt_regs *ctx, uint32_t event_type) {
  u64 id = bpf_get_current_pid_tgid();

  int64_t written_bytes = PT_REGS_RC(ctx);
  if (written_bytes < 0) {
    // This write() call failed.
    goto done;
  }

  const struct write_info_t* write_info = active_write_info_map.lookup(&id);
  if (write_info == NULL) {
    goto done;
  }

  uint64_t lookup_fd = write_info->lookup_fd;
  struct accept_info_t *accept_info = accept_info_map.lookup(&lookup_fd);
  if (accept_info == NULL) {
    goto done;
  }

  u32 zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == NULL) {
    goto done;
  }

  // TODO(oazizi/yzhao): Why does the commented line not work? Error message is below:
  //                     R2 min value is negative, either use unsigned or 'var &= const'
  //                     Need to bring this back to avoid unnecessary fix-up in userspace.
  //size_t buf_size = count < sizeof(event->msg) ? count : sizeof(event->msg);
  size_t buf_size = sizeof(event->msg);

  event->attr.accept_info = *accept_info;
  // Increment sequence number after copying so the index is 0-based.
  ++accept_info->seq_num;
  event->attr.event_type = event_type;
  event->attr.msg_bytes = written_bytes;
  // TODO(yzhao): This is the time is after write/send() finishes. If we want to capture the time
  // before write/send() starts, we need to capture the time in probe_entry_write_send().
  event->attr.time_stamp_ns = bpf_ktime_get_ns();
  event->attr.tgid = id >> 32;
  event->attr.pid = (uint32_t)id;
  event->attr.fd = (uint32_t)write_info->lookup_fd;
  event->attr.msg_buf_size = buf_size;

  bpf_probe_read(&event->msg, buf_size, (const void*) write_info->buf);

  // Write snooped arguments to perf ring buffer.
  unsigned int size_to_submit = sizeof(event->attr) + buf_size;
  syscall_write_events.perf_submit(ctx, event, size_to_submit);

 done:
  // Regardless of what happened, after write/send/sendto() returns, there is no need to track the fd & buf
  // being accepted by write/send/sendto() call, therefore we can remove the entry.
  active_write_info_map.delete(&id);
  return 0;
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

int probe_entry_sendto(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_entry_write_send(ctx, fd, buf, count);
}

int probe_ret_sendto(struct pt_regs *ctx) {
  return probe_ret_write_send(ctx, kEventTypeSyscallSendEvent);
}

// TODO(oazizi): Look into the following opens:
// 1) Should we trace sendmsg(), which is another syscall, but with a different interface?
// 2) Why does the syscall table only include sendto, while Linux source code and man page list both sendto and send?
// 3) What do we do when the sendto() is called with a dest_addr provided? I believe this overrides the accept_info.

int probe_close(struct pt_regs *ctx, int fd) {
  if (fd < 0) { return 0; }
  u64 id = bpf_get_current_pid_tgid();
  u32 tgid = id >> 32;
  u64 lookup_fd = ((u64)tgid << 32) | fd;
  accept_info_map.delete(&lookup_fd);
  return 0;
}
