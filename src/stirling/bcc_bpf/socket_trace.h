#pragma once

#ifdef __cplusplus
#include <cstdint>
#endif

#include <linux/in6.h>

// Indicates that the event is for a write() syscall, whose msg field records the input data
// to write().
const uint32_t kEventTypeSyscallWriteEvent = 1;
const uint32_t kEventTypeSyscallSendEvent = 2;

// Protocol being used on a connection (HTTP, MySQL, etc.).
const uint32_t kProtocolUnknown = 0;
const uint32_t kProtocolHTTP = 1;
const uint32_t kProtocolMySQL = 2;

// This struct contains information collected when a connection is established,
// via an accept() syscall.
// This struct must be aligned, because BCC cannot currently handle unaligned structs.
struct conn_info_t {
  uint64_t timestamp_ns;
  struct sockaddr_in6 addr;
  // A 0-based number that uniquely identify a connection for a process.
  uint32_t conn_id;
  // The protocol on the connection (HTTP, MySQL, etc.)
  uint32_t protocol;
  // A 0-based number for the next event on this connection.
  // This number is incremented each time a new event is recorded.
  uint64_t seq_num;
} __attribute__((__packed__, aligned(8)));

// This is the maximum value for the msg size.
// This is use for experiment dealing with large message
// tracing on a vfs_write.
#define MAX_MSG_SIZE 4096

struct socket_data_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  struct attr_t {
    // Information from the accept() syscall, including IP and port.
    struct conn_info_t conn_info;
    // The time stamp as this is captured by BPF program.
    uint64_t time_stamp_ns;
    // Comes from the process from which this is captured.
    // See https://stackoverflow.com/a/9306150 for details.
    uint32_t tgid;
    uint32_t pid;
    // The file descriptor to the opened network connection.
    uint32_t fd;
    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    uint32_t event_type;
    // The size of the original data being captured, which might be truncated because msg
    // field has a size list.
    uint32_t msg_bytes;
    // The size of the data being actually captured in msg field.
    // Care need to be take as only msg_size bytes of msg are guaranteed to be valid.
    uint32_t msg_buf_size;
  } attr;
  char msg[MAX_MSG_SIZE];
} __attribute__((__packed__));
