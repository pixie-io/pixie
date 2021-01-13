#include "src/stirling/source_connectors/socket_tracer/fd_resolver.h"

#include <chrono>

#include "src/common/base/base.h"
#include "src/common/fs/inode_utils.h"

namespace pl {
namespace stirling {

FDResolver::FDResolver(system::ProcParser* proc_parser, int pid, int fd)
    : proc_parser_(proc_parser), pid_(pid), fd_(fd) {}

bool FDResolver::Setup() {
  // Record some information about the FD.
  // This marks the starting point at which we reliably know the connection.
  // We won't be able to infer the connection info this time, but
  // the hope is that we can recover the socket information on the next iteration,
  // if the connection appears to be stable.

  Status s = proc_parser_->ReadProcPIDFDLink(pid_, fd_, &fd_link_);
  if (!s.ok()) {
    VLOG(2) << absl::Substitute("Can't set-up connection inference [msg=$0].", s.msg());
    active_ = false;
    return false;
  }

  VLOG(2) << absl::Substitute("Set-up connection inference: $0", fd_link_);
  // Record the time slightly after recording the FD, so we have a more conservative
  // time window. We don't want false positives.
  first_timestamp_ = std::chrono::steady_clock::now();
  active_ = true;
  return true;
}

bool FDResolver::Update() {
  ECHECK(active_) << "FDResolver must be in active state.";
  ECHECK(!fd_link_.empty()) << "Candidate FD link should not be empty";

  // Record the timestamp. Must be done before reading /proc,
  // to avoid a race where we find the /proc FD entry, then the FD closes, then we grab the
  // timestamp. This would result in having an incorrect window of time during which the FD was
  // valid.
  std::chrono::time_point<std::chrono::steady_clock> timestamp = std::chrono::steady_clock::now();

  std::string current_fd_link;
  Status s = proc_parser_->ReadProcPIDFDLink(pid_, fd_, &current_fd_link);
  if (!s.ok()) {
    VLOG(2) << "Can't infer remote endpoint. FD is not accessible.";
    active_ = false;
    return false;
  }

  if (current_fd_link != fd_link_) {
    VLOG(2) << "Can't infer remote endpoint. FD link has changed, implying connection has closed.";
    active_ = false;
    return false;
  }

  last_timestamp_ = std::move(timestamp);
  return true;
}

std::optional<std::string_view> FDResolver::InferFDLink(
    std::chrono::time_point<std::chrono::steady_clock> time) {
  if (time > first_timestamp_ && time < last_timestamp_) {
    return fd_link_;
  }
  return {};
}

}  // namespace stirling
}  // namespace pl
