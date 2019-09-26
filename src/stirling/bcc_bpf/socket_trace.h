#pragma once

#ifdef __cplusplus
#include <cstdint>
#endif

#include <linux/in6.h>

// TODO(yzhao): According to https://github.com/cilium/cilium/blob/master/Documentation/bpf.rst
// and https://lwn.net/Articles/741773/, kernel 4.16 & llvm 6.0 or newer are required to support BPF
// to BPF calls for C code. Figure out how to detect kernel and llvm versions.
#ifndef __inline
#ifdef SUPPORT_BPF2BPF_CALL
#define __inline
#else
// TODO(yzhao): Clarify the effect on GCC and Clang, and see if we can remove the inline keyword.
#define __inline inline __attribute__((__always_inline__))
#endif  // SUPPORT_BPF2BPF_CALL
#endif  // __inline

const char kControlMapName[] = "control_map";
const uint64_t kSocketTraceNothing = 0;

const int64_t kTraceAllTGIDs = -1;
const char kControlValuesArrayName[] = "control_values";

const char kStmtPreparePrefix = '\x16';
const char kStmtExecutePrefix = '\x17';
const char kStmtClosePrefix = '\x19';
const char kQueryPrefix = '\x03';

const size_t kHTTP2FrameHeaderSizeInBytes = 9;

// TODO(yzhao): Investigate the performance cost of misaligned memory access (8 vs. 4 bytes).

typedef enum {
  kEgress,
  kIngress,
} TrafficDirection;

// Protocol being used on a connection (HTTP, MySQL, etc.).
typedef enum {
  kProtocolUnknown,
  kProtocolHTTP,
  kProtocolHTTP2,
  kProtocolMySQL,
  kNumProtocols
} TrafficProtocol;

// The direction of traffic expected on a probe. Values are used in bit masks.
typedef enum {
  kRoleUnknown = 1 << 0,
  kRoleRequestor = 1 << 1,
  kRoleResponder = 1 << 2
} ReqRespRole;

// Specifies the corresponding indexes of the entries of a per-cpu array.
typedef enum {
  // This specify one pid to monitor. This is used during test to eliminate noise.
  // TODO(yzhao): We need a more robust mechanism for production use, which should be able to:
  // * Specify multiple pids up to a certain limit, let's say 1024.
  // * Support efficient lookup inside bpf to minimize overhead.
  kTargetTGIDIndex = 0,
  kStirlingTGIDIndex,
  kNumControlValues,
} ControlValueIndex;

struct traffic_class_t {
  // The protocol of traffic on the connection (HTTP, MySQL, etc.).
  TrafficProtocol protocol;
  // Classify traffic as requests, responses or mixed.
  ReqRespRole role;
};

struct conn_id_t {
  // Comes from the process from which this is captured.
  // See https://stackoverflow.com/a/9306150 for details.
  // Use union to give it two names. We use tgid in kernel-space, pid in user-space.
  union {
    uint32_t tgid;
    uint32_t pid;
  };
  // The start time of the PID, so we can disambiguate PIDs.
  union {
    uint64_t tgid_start_time_ns;
    uint64_t pid_start_time_ns;
  };
  // The file descriptor to the opened network connection.
  uint32_t fd;
  // Generation number of the FD (increments on each FD reuse in the TGID).
  uint32_t generation;
};

// This struct contains information collected when a connection is established,
// via an accept() syscall.
// This struct must be aligned, because BCC cannot currently handle unaligned structs.
struct conn_info_t {
  uint64_t timestamp_ns;
  // Connection identifier (PID, FD, etc.).
  struct conn_id_t conn_id;
  // IP address of the remote endpoint.
  struct sockaddr_in6 addr;

  // The protocol and message type of traffic on the connection (HTTP/Req, HTTP/Resp, MySQL/Req,
  // etc.).
  struct traffic_class_t traffic_class;
  int protocol_observed_count;

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
    // The time stamp as this is captured by BPF program.
    uint64_t timestamp_ns;
    // Connection identifier (PID, FD, etc.).
    struct conn_id_t conn_id;
    // The protocol on the connection (HTTP, MySQL, etc.), and the server-client role.
    struct traffic_class_t traffic_class;
    // The type of the actual data that the msg field encodes, which is used by the caller
    // to determine how to interpret the data.
    TrafficDirection direction;
    // A 0-based sequence number for this event on the connection.
    // Note that write/send have separate sequences than read/recv.
    uint64_t seq_num;
    // The size of the original message. We use this to truncate msg field to minimize the amount
    // of data being transferred. This data available in msg buffer is min(msg_size, MAX_MSG_SIZE).
    uint32_t msg_size;
  } attr;
  char msg[MAX_MSG_SIZE];
};
