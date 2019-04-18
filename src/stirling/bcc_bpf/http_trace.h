#pragma once

// This is the maximum value for the msg size.
// This is use for experiment dealing with large message
// tracing on a vfs_write.
#define MAX_MSG_SIZE 4096

struct syscall_write_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  struct attr_t {
    // The time stamp as this is captured by BPF program.
    unsigned int time_stamp_ns;
    // Comes from the process from which this is captured.
    unsigned int pid_tgid;
    // The file descriptor to the opened network connection.
    int fd;
    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    unsigned int event_type;
    // The size of the original data being captured, which might be truncated because msg
    // field has a size list.
    unsigned int bytes;
    // The size of the data being actually captured in msg field.
    // Care need to be take as only msg_size bytes of msg are guaranteed to be valid.
    unsigned int msg_size;
  } attr;
  char msg[MAX_MSG_SIZE];
} __attribute__((__packed__));

// Indicates that the event is for a write() syscall, whose msg field records the input data
// to write().
const int kEventTypeSyscallWriteEvent = 1;
// Indicates that the event is for a accept() syscall, whose msg field records the resultant
// sockaddr data structure.
const int kEventTypeSyscallAddrEvent = 2;
