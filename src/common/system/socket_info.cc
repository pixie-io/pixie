#include "src/common/system/socket_info.h"

#include <arpa/inet.h>
#include <errno.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <utility>

namespace pl {
namespace system {

// See tcp_states.h for other states if we ever need them.
constexpr int kTCPEstablishedState = 1;

NetlinkSocketProber::NetlinkSocketProber() {
  fd_ = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_SOCK_DIAG);
  ECHECK(fd_ > 0) << absl::Substitute("Could not create NETLINK_SOCK_DIAG connection. [fd=$0]",
                                      fd_);
}

NetlinkSocketProber::~NetlinkSocketProber() {
  if (fd_ > 0) {
    close(fd_);
  }
}

StatusOr<std::unique_ptr<std::map<int, SocketInfo>>> NetlinkSocketProber::InetConnections() {
  PL_RETURN_IF_ERROR(SendInetDiagReq());
  return RecvInetDiagResp();
}

Status NetlinkSocketProber::SendInetDiagReq() {
  ssize_t msg_len = sizeof(struct nlmsghdr) + sizeof(struct inet_diag_req_v2);

  struct nlmsghdr msg_header = {};
  msg_header.nlmsg_len = msg_len;
  msg_header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  msg_header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  struct inet_diag_req_v2 msg_req = {};
  msg_req.sdiag_family = AF_INET;
  msg_req.sdiag_protocol = IPPROTO_TCP;
  msg_req.idiag_states = 1 << kTCPEstablishedState;

  struct iovec iov[2];
  iov[0].iov_base = &msg_header;
  iov[0].iov_len = sizeof(msg_header);
  iov[1].iov_base = &msg_req;
  iov[1].iov_len = sizeof(msg_req);

  struct sockaddr_nl nl_addr = {};
  nl_addr.nl_family = AF_NETLINK;

  struct msghdr msg = {};
  msg.msg_name = &nl_addr;
  msg.msg_namelen = sizeof(nl_addr);
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;

  ssize_t bytes_sent = 0;

  while (bytes_sent < msg_len) {
    ssize_t retval = sendmsg(fd_, &msg, 0);
    if (retval < 0) {
      return error::Internal("Failed to send NetLink messages [errno=$0]", errno);
    }
    bytes_sent += retval;
  }

  return Status::OK();
}

namespace {

Status ProcessInetDiagMsg(const struct inet_diag_msg& diag_msg, unsigned int len,
                          std::map<int, SocketInfo>* socket_info_entries) {
  if (len < NLMSG_LENGTH(sizeof(diag_msg))) {
    return error::Internal("Not enough bytes");
  }

  if (diag_msg.idiag_family != AF_INET && diag_msg.idiag_family != AF_INET6) {
    return error::Internal("Unsupported address family $0", diag_msg.idiag_family);
  }

  auto iter = socket_info_entries->find(diag_msg.idiag_inode);

  ECHECK(iter == socket_info_entries->end())
      << absl::Substitute("Clobbering socket info at inode=$0", diag_msg.idiag_inode);

  SocketInfo socket_info = {};
  socket_info.family = diag_msg.idiag_family;
  socket_info.local_port = diag_msg.id.idiag_sport;
  socket_info.local_addr = *reinterpret_cast<const struct in6_addr*>(&diag_msg.id.idiag_src);
  socket_info.remote_port = diag_msg.id.idiag_dport;
  socket_info.remote_addr = *reinterpret_cast<const struct in6_addr*>(&diag_msg.id.idiag_dst);

  socket_info_entries->insert({diag_msg.idiag_inode, std::move(socket_info)});

  return Status::OK();
}

}  // namespace

StatusOr<std::unique_ptr<std::map<int, SocketInfo>>> NetlinkSocketProber::RecvInetDiagResp() {
  auto socket_info_entries = std::make_unique<std::map<int, SocketInfo>>();

  static constexpr int kBufSize = 8192;
  uint8_t buf[kBufSize];

  bool done = false;
  while (!done) {
    ssize_t num_bytes = recv(fd_, &buf, sizeof(buf), 0);
    if (num_bytes < 0) {
      return error::Internal("Receive call failed");
    }

    struct nlmsghdr* msg_header = reinterpret_cast<struct nlmsghdr*>(buf);

    for (; NLMSG_OK(msg_header, num_bytes); msg_header = NLMSG_NEXT(msg_header, num_bytes)) {
      if (msg_header->nlmsg_type == NLMSG_DONE) {
        done = true;
        break;
      }

      if (msg_header->nlmsg_type == NLMSG_ERROR) {
        return error::Internal("Netlink error");
      }

      if (msg_header->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
        return error::Internal("Unexpected message type");
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
      struct inet_diag_msg* diag_msg =
          reinterpret_cast<struct inet_diag_msg*>(NLMSG_DATA(msg_header));
#pragma GCC diagnostic pop
      PL_RETURN_IF_ERROR(
          ProcessInetDiagMsg(*diag_msg, msg_header->nlmsg_len, socket_info_entries.get()));
    }
  }

  return socket_info_entries;
}

}  // namespace system
}  // namespace pl
