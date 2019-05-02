#include <uapi/linux/ptrace.h>
#include <uapi/linux/in6.h>
#include <linux/socket.h>

// This is copied from http_trace.h, with comments removed, so that this whole file can be
// hermetically installed as a BPF program.

// TODO(yzhao): PL-451 These struct definitions should come
// from a header file. Figure out how to expand the header file
// using preprocessing in pl_cc_resource.

struct accept_info_t {
  uint64_t timestamp_ns;
  struct sockaddr_in6 addr;
} __attribute__((__packed__, aligned(8)));

#define MAX_MSG_SIZE 4096
struct syscall_write_event_t {
  struct attr_t {
    struct accept_info_t accept_info;
    uint64_t time_stamp_ns;
    // Comes from the process from which this is captured.
    uint32_t tgid;
    uint32_t pid;
    int fd;
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

// Tracks struct addr_info so we can map between entry and exit.
// Key is {TGID, fd}.
BPF_HASH(active_sock_addr, u64, struct addr_info_t);

// Map recording connection-related information on connection accept.
// Key is {TGID, fd}.
BPF_HASH(accept_info_map, u64, struct accept_info_t);


// This function stores the address to the sockaddr struct in the active_sock_addr map.
// The key is the current pid/tgid.
//
// TODO(yzhao): We are not able to trace the source address/port yet. We might need access the
// sockfd argument to accept() syscall.
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
  u64 tgid = id >> 32;

  u64 ret_fd = PT_REGS_RC(ctx);
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
  ret_fd = (tgid << 32) | ret_fd;

  struct accept_info_t accept_info;
  memset(&accept_info, 0, sizeof(struct accept_info_t));
  accept_info.timestamp_ns = bpf_ktime_get_ns();
  accept_info.addr = *((struct sockaddr_in6*) addr_info->addr);
  accept_info_map.update(&ret_fd, &accept_info);

 done:
  // Regardless of what happened, after accept() returns, there is no need to track the sock_addr
  // being accepted during the accept() call, therefore we can remove the entry.
  active_sock_addr.delete(&id);
  return 0;
}

static int probe_write_send(struct pt_regs *ctx, int fd, char* buf, size_t count, uint32_t event_type) {
  u32 zero = 0;

  u64 id = bpf_get_current_pid_tgid();
  u64 tgid = id >> 32;
  u64 lookup_fd = (tgid << 32) | fd;

  struct accept_info_t *accept_info = accept_info_map.lookup(&lookup_fd);
  if (accept_info == NULL) {
    // Bail early if we aren't tracking fd.
    return 0;
  }

  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == NULL) {
    return 0;
  }

  // TODO(oazizi/yzhao): Why does the commented line not work? Error message is below:
  //                     R2 min value is negative, either use unsigned or 'var &= const'
  //                     Need to bring this back to avoid unnecessary fix-up in userspace.
  //size_t buf_size = count < sizeof(event->msg) ? count : sizeof(event->msg);
  size_t buf_size = sizeof(event->msg);

  event->attr.accept_info = *accept_info;
  event->attr.event_type = event_type;
  event->attr.msg_bytes = count;
  event->attr.time_stamp_ns = bpf_ktime_get_ns();
  event->attr.tgid = id >> 32;
  event->attr.pid = (uint32_t)id;
  event->attr.fd = fd;
  event->attr.msg_buf_size = buf_size;

  bpf_probe_read(&event->msg, buf_size, (const void*) buf);

  // Write snooped arguments to perf ring buffer.
  unsigned int size_to_submit = sizeof(event->attr) + buf_size;
  syscall_write_events.perf_submit(ctx, event, size_to_submit);

  return 0;
}

int probe_write(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_write_send(ctx, fd, buf, count, kEventTypeSyscallWriteEvent);
}

int probe_send(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  return probe_write_send(ctx, fd, buf, count, kEventTypeSyscallSendEvent);
}



int probe_close(struct pt_regs *ctx, int fd) {
  u64 id = bpf_get_current_pid_tgid();
  u64 tgid = id >> 32;
  u64 lookup_fd = (tgid << 32) | fd;
  accept_info_map.delete(&lookup_fd);
  return 0;
}
