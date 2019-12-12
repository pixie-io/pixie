#pragma once

#include <linux/in6.h>

#include "src/stirling/bcc_bpf_interface/common.h"

const char kControlMapName[] = "control_map";
const uint64_t kSocketTraceNothing = 0;

const int64_t kTraceAllTGIDs = -1;
const char kControlValuesArrayName[] = "control_values";

// TODO(yzhao): Investigate the performance cost of misaligned memory access (8 vs. 4 bytes).

// This struct contains information collected when a connection is established,
// via an accept() syscall.
struct conn_info_t {
  // Connection identifier (PID, FD, etc.).
  struct conn_id_t conn_id;

  // IP address of the remote endpoint.
  struct sockaddr_in6 addr;

  // The protocol and message type of traffic on the connection (HTTP/Req, HTTP/Resp, MySQL/Req,
  // etc.).
  struct traffic_class_t traffic_class;

  // TODO(yzhao): Following fields are only internal tracking only, and is not needed when
  // submitting a new connection. Consider separate these data with the above data that is pushed to
  // perf buffer.

  // TODO(oazizi/yzhao/PL-935): Convert these seq_num to byte positions. This will help in recovery
  // when there is a lost event.

  // A 0-based number for the next write event on this connection.
  // This number is incremented each time a new write event is recorded.
  uint64_t wr_seq_num;
  // A 0-based number for the next read event on this connection.
  // This number is incremented each time a new read event is recorded.
  uint64_t rd_seq_num;

  // The offset to start read the first frame of the upcoming bytes buffer.
  uint64_t wr_next_http2_frame_offset;
  uint64_t rd_next_http2_frame_offset;

  // Some stats for protocol inference. Used for threshold-based filtering.
  int32_t protocol_match_count;
  int32_t protocol_total_count;
};

// This struct is a subset of conn_info_t. It is used to communicate connect/accept events.
// See conn_info_t for descriptions of the members.
struct conn_event_t {
  uint64_t timestamp_ns;     // Must be shared with close_event_t.
  struct conn_id_t conn_id;  // Must be shared with close_event_t.

  struct sockaddr_in6 addr;
  struct traffic_class_t traffic_class;
};

// This struct is a subset of conn_info_t. It is used to communicate close events.
// See conn_info_t for descriptions of the members.
struct close_event_t {
  uint64_t timestamp_ns;     // Must be shared with conn_event_t.
  struct conn_id_t conn_id;  // Must be shared with conn_event_t.

  uint64_t wr_seq_num;
  uint64_t rd_seq_num;
};

// Data buffer message size. BPF can submit at most this amount of data to a perf buffer.
//
// NOTE: This size does not directly affect the size of perf buffer submits, as the actual data
// submitted to perf buffers are determined by attr.msg_size. In cases where socket_data_event_t
// is defined as stack variable, the size can be problematic. Currently we only have a few instances
// in *_test.cc files.
//
// TODO(yzhao): We do not yet have a good sense of the desired size. Things to consider:
// * Overhead. This single instance is small. However, we should consider this in the context of all
// possible overhead in BPF program.
// * Complexity. If this buffer is not sufficiently large. We'll need to handle chunked message
// inside user space parsing code.
// ATM, we saw in one case, when gRPC reflection RPC itself is invoked, it can send one
// FileDescriptorProto [1], which often become large. That's the only data point we have right now.
//
// [1] https://github.com/grpc/grpc-go/blob/master/reflection/serverreflection.go
#define MAX_MSG_SIZE 16384  // 16KiB

// This defines how many chunks a perf_submit can support.
// This applies to messages that are over MAX_MSG_SIZE,
// and effecitvely makes the maximum message size to be CHUNK_LIMIT*MAX_MSG_SIZE.
#define CHUNK_LIMIT 4

struct socket_data_event_t {
  // We split attributes into a separate struct, because BPF gets upset if you do lots of
  // size arithmetic. This makes it so that it's attributes followed by message.
  //
  // TODO(yzhao): When adding http2_frame_offset, use a union, so that it allows adding similar data
  // for other protocols.
  struct attr_t {
    // The time stamp when syscall entry probe was triggered.
    uint64_t entry_timestamp_ns;
    // The time stamp when syscall return probe was triggered.
    uint64_t return_timestamp_ns;
    // Connection identifier (PID, FD, etc.).
    struct conn_id_t conn_id;
    // The protocol on the connection (HTTP, MySQL, etc.), and the server-client role.
    struct traffic_class_t traffic_class;
    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    enum TrafficDirection direction;
    // A 0-based sequence number for this event on the connection.
    // Note that write/send have separate sequences than read/recv.
    uint64_t seq_num;
    // The size of the original message. We use this to truncate msg field to minimize the amount
    // of data being transferred. This data available in msg buffer is min(msg_size, MAX_MSG_SIZE).
    uint32_t msg_size;
  } attr;
  char msg[MAX_MSG_SIZE];
};

typedef enum {
  kConnOpen,
  kConnClose,
} ControlEventType;

struct socket_control_event_t {
  ControlEventType type;
  union {
    struct conn_event_t open;
    struct close_event_t close;
  };
};
