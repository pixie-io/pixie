package main


const bpfProgram = `
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

#define MAX_MSG_SIZE 4096

BPF_PERF_OUTPUT(syscall_write_events);

struct syscall_write_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  struct attr_t {
    unsigned int event_type;
    unsigned int bytes;
    // Care need to be take as only msg_size bytes of msg are guaranteed
    // to be valid.
    unsigned int msg_size;
  } attr;
  char msg[MAX_MSG_SIZE];
} __attribute__((__packed__));

const int kEventTypeSyscallWriteEvent = 1;
const int kEventTypeSyscallAddrEvent = 2;

// The set of file descriptors we are tracking.
BPF_HASH(active_fds, int, bool);

// Tracks struct addr_info so we can map between entry and exit.
BPF_HASH(active_sock_addr, u64, struct addr_info_t);


// This function stores the address to the sockaddr struct in the active_sock_addr map.
// The key is the current pid/tgid.
int probe_entry_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  if (pid != $PID) {
      return 0;
  }

  struct addr_info_t addr_info;
  addr_info.addr = addr;
  addr_info.addrlen = addrlen;
  active_sock_addr.update(&id, &addr_info);

  return 0;
}


// Read the sockaddr values and write to the output buffer.
int probe_ret_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  if (pid != $PID) {
      return 0;
  }

  struct addr_info_t* addr_info = active_sock_addr.lookup(&id);
  if (addr_info == NULL) {
    goto done;
  }

  int ret = PT_REGS_RC(ctx);
  if (ret < 0) {
    goto done;
  }
  bool t = true;
  active_fds.update(&ret, &t);

  int zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == 0) {
    goto done;
  }

  u64 addr_size = *(addr_info->addrlen);
  size_t buf_size = addr_size < sizeof(event->msg) ? addr_size : sizeof(event->msg);
  bpf_probe_read(&event->msg, buf_size, addr_info->addr);
  event->attr.event_type = kEventTypeSyscallAddrEvent;
  event->attr.msg_size = buf_size;
  event->attr.bytes = buf_size;
  unsigned int size_to_submit = sizeof(event->attr) + buf_size;
  syscall_write_events.perf_submit(ctx, event, size_to_submit);

 done:
  active_sock_addr.delete(&id);
  return 0;
}

int probe_write(struct pt_regs *ctx, int fd, char* buf, size_t count) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  if (pid != $PID) {
      return 0;
  }

  if (active_fds.lookup(&fd) == NULL) {
    // Bail early if we aren't tracking fd.
    return 0;
  }
  int zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == 0) {
    return 0;
  }

  event->attr.bytes = count;
  size_t buf_size = count < sizeof(event->msg) ? count : sizeof(event->msg);
  bpf_probe_read(&event->msg, buf_size, (void*) buf);
  event->attr.msg_size = buf_size;

  unsigned int size_to_submit = sizeof(event->attr) + buf_size;
  event->attr.event_type = kEventTypeSyscallWriteEvent;
  // Write snooped arguments to perf ring buffer.
  syscall_write_events.perf_submit(ctx, event, size_to_submit);

  return 0;

}

int probe_close(struct pt_regs *ctx, int fd) {
  active_fds.delete(&fd);
  return 0;
}
`
