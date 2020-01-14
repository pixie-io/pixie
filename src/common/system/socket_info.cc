#include "src/common/system/socket_info.h"

#include <arpa/inet.h>
#include <linux/in6.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/unix_diag.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <utility>

#include "src/common/base/base.h"

namespace pl {
namespace system {

NetlinkSocketProber::NetlinkSocketProber() {
  fd_ = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_SOCK_DIAG);
  if (fd_ < 0) {
    LOG(DFATAL) << absl::Substitute("Could not create NETLINK_SOCK_DIAG connection. [errno=$0]",
                                    errno);
    return;
  }
}

NetlinkSocketProber::~NetlinkSocketProber() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

template <typename TDiagReqType>
Status NetlinkSocketProber::SendDiagReq(const TDiagReqType& msg_req) {
  ssize_t msg_len = sizeof(struct nlmsghdr) + sizeof(TDiagReqType);

  struct nlmsghdr msg_header = {};
  msg_header.nlmsg_len = msg_len;
  msg_header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  msg_header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  struct iovec iov[2];
  iov[0].iov_base = &msg_header;
  iov[0].iov_len = sizeof(msg_header);
  iov[1].iov_base = const_cast<TDiagReqType*>(&msg_req);
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

Status ProcessDiagMsg(const struct inet_diag_msg& diag_msg, unsigned int len,
                      std::map<int, SocketInfo>* socket_info_entries) {
  if (len < NLMSG_LENGTH(sizeof(diag_msg))) {
    return error::Internal("Not enough bytes");
  }

  if (diag_msg.idiag_family != AF_INET && diag_msg.idiag_family != AF_INET6) {
    return error::Internal("Unsupported address family $0", diag_msg.idiag_family);
  }

  if (diag_msg.idiag_inode == 0) {
    // TODO(PL-1001): Investigate why inode of 0 is intermittently produced.
    // Shouldn't happen since we ask for for established connections only.
    LOG(WARNING) << "Did not expect inode of 0 for established connections...ignoring it.";
    return Status::OK();
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

Status ProcessDiagMsg(const struct unix_diag_msg& diag_msg, unsigned int len,
                      std::map<int, SocketInfo>* socket_info_entries) {
  if (len < NLMSG_LENGTH(sizeof(diag_msg))) {
    return error::Internal("Not enough bytes");
  }

  if (diag_msg.udiag_family != AF_UNIX) {
    return error::Internal("Unsupported address family $0", diag_msg.udiag_family);
  }

  // Since we asked for UDIAG_SHOW_PEER in the unix_diag_req,
  // The response has additional attributes, which we parse here.
  // In particular, we are looking for the peer socket's inode number.
  const struct rtattr* attr;
  unsigned int rta_len = len - NLMSG_LENGTH(sizeof(diag_msg));
  unsigned int peer = 0;

  for (attr = reinterpret_cast<const struct rtattr*>(&diag_msg + 1); RTA_OK(attr, rta_len);
       attr = RTA_NEXT(attr, rta_len)) {
    switch (attr->rta_type) {
      case UNIX_DIAG_NAME:
        // Nothing for now, but could extract path name, if needed.
        // For this to work, one should also add UDIAG_SHOW_NAME to the request.
        break;
      case UNIX_DIAG_PEER:
        if (RTA_PAYLOAD(attr) >= sizeof(peer)) {
          peer = *reinterpret_cast<unsigned int*>(RTA_DATA(attr));
        }
    }
  }

  auto iter = socket_info_entries->find(diag_msg.udiag_ino);
  ECHECK(iter == socket_info_entries->end())
      << absl::Substitute("Clobbering socket info at inode=$0", diag_msg.udiag_ino);

  SocketInfo socket_info = {};
  socket_info.family = diag_msg.udiag_family;
  socket_info.local_port = diag_msg.udiag_ino;
  socket_info.local_addr = {};
  socket_info.remote_port = peer;
  socket_info.remote_addr = {};

  socket_info_entries->insert({diag_msg.udiag_ino, std::move(socket_info)});

  return Status::OK();
}

}  // namespace

template <typename TDiagMsgType>
Status NetlinkSocketProber::RecvDiagResp(std::map<int, SocketInfo>* socket_info_entries) {
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
      TDiagMsgType* diag_msg = reinterpret_cast<TDiagMsgType*>(NLMSG_DATA(msg_header));
#pragma GCC diagnostic pop
      PL_RETURN_IF_ERROR(ProcessDiagMsg(*diag_msg, msg_header->nlmsg_len, socket_info_entries));
    }
  }

  return Status::OK();
}

Status NetlinkSocketProber::InetConnections(std::map<int, SocketInfo>* socket_info_entries,
                                            int conn_states) {
  struct inet_diag_req_v2 msg_req = {};
  msg_req.sdiag_family = AF_INET;
  msg_req.sdiag_protocol = IPPROTO_TCP;
  msg_req.idiag_states = conn_states;

  PL_RETURN_IF_ERROR(SendDiagReq(msg_req));
  PL_RETURN_IF_ERROR(RecvDiagResp<struct inet_diag_msg>(socket_info_entries));
  return Status::OK();
}

Status NetlinkSocketProber::UnixConnections(std::map<int, SocketInfo>* socket_info_entries,
                                            int conn_states) {
  struct unix_diag_req msg_req = {};
  msg_req.sdiag_family = AF_UNIX;
  msg_req.udiag_states = conn_states;
  msg_req.udiag_show = UDIAG_SHOW_PEER;

  PL_RETURN_IF_ERROR(SendDiagReq(msg_req));
  PL_RETURN_IF_ERROR(RecvDiagResp<struct unix_diag_msg>(socket_info_entries));
  return Status::OK();
}

}  // namespace system
}  // namespace pl
