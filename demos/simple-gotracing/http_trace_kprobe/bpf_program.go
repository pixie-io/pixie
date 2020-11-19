package main

const bpfProgram = `
#include <linux/sched.h>
#include <uapi/linux/ptrace.h>
#include <uapi/linux/stat.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/stat.h>
#include <linux/socket.h>

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

#define MAX_MSG_SIZE 1024

BPF_PERF_OUTPUT(syscall_write_events);

// This needs to match exactly with the Go version of the struct.
struct syscall_write_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  struct attr_t {
    int event_type;
    int fd;
    int bytes;
    // Care needs to be taken as only msg_size bytes of msg are guaranteed
    // to be valid.
    int msg_size;
  } attr;
  char msg[MAX_MSG_SIZE];
};

const int kEventTypeSyscallAddrEvent = 1;
const int kEventTypeSyscallWriteEvent = 2;
const int kEventTypeSyscallCloseEvent = 3;

// BPF programs are limited to a 512-byte stack. We store this value per CPU
// and use it as a heap allocated value.
BPF_PERCPU_ARRAY(write_buffer_heap, struct syscall_write_event_t, 1);      

// The set of file descriptors we are tracking.
BPF_HASH(active_fds, int, bool);

// Tracks struct addr_info so we can map between entry and exit.
BPF_HASH(active_sock_addr, u64, struct addr_info_t);


// This function stores the address to the sockaddr struct in the active_sock_addr map.
// The key is the current pid/tgid.
int syscall__probe_entry_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen, int flags) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  // $PID is substituted with PID before probe is inserted.
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
int syscall__probe_ret_accept4(struct pt_regs *ctx, int sockfd, struct sockaddr *addr, size_t *addrlen, int flags) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  // $PID is substituted with PID before probe is inserted.
  if (pid != $PID) {
      return 0;
  }

  struct addr_info_t* addr_info = active_sock_addr.lookup(&id);
  if (addr_info == NULL) {
    goto done;
  }

  // The file descriptor is the value returned from the syscall.
  int fd = PT_REGS_RC(ctx);
  if (fd < 0) {
    goto done;
  }
  bool t = true;
  active_fds.update(&fd, &t);

  int zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == 0) {
    goto done;
  }

  u64 addr_size = *(addr_info->addrlen);
  size_t buf_size = addr_size < sizeof(event->msg) ? addr_size : sizeof(event->msg);
  bpf_probe_read(&event->msg, buf_size, addr_info->addr);
  event->attr.event_type = kEventTypeSyscallAddrEvent;
  event->attr.fd = fd;
  event->attr.msg_size = buf_size;
  event->attr.bytes = buf_size;
  unsigned int size_to_submit = sizeof(event->attr) + buf_size;
  syscall_write_events.perf_submit(ctx, event, size_to_submit);

 done:
  active_sock_addr.delete(&id);
  return 0;
}

int syscall__probe_write(struct pt_regs *ctx, int fd, const void* buf, size_t count) {
  int zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == NULL) {
    return 0;
  }

  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  // $PID is substituted with PID before probe is inserted.
  if (pid != $PID) {
      return 0;
  }

  if (active_fds.lookup(&fd) == NULL) {
    // Bail early if we aren't tracking fd.
    return 0;
  }

  event->attr.fd = fd;
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

int syscall__probe_close(struct pt_regs *ctx, int fd) {
  u64 id = bpf_get_current_pid_tgid();
  u32 pid = id >> 32;
  // $PID is substituted with PID before probe is inserted.
  if (pid != $PID) {
      return 0;
  }

  int zero = 0;
  struct syscall_write_event_t *event = write_buffer_heap.lookup(&zero);
  if (event == NULL) {
    return 0;
  }
  
  event->attr.event_type = kEventTypeSyscallCloseEvent;
  event->attr.fd = fd;
  event->attr.bytes = 0;
  event->attr.msg_size = 0;
  syscall_write_events.perf_submit(ctx, event, sizeof(event->attr));

  active_fds.delete(&fd);
  return 0;
}
`
