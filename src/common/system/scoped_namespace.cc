#include "src/common/system/scoped_namespace.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "src/common/system/config.h"

namespace pl {
namespace system {

StatusOr<std::unique_ptr<ScopedNamespace>> ScopedNamespace::Create(int ns_pid,
                                                                   std::string_view ns_type) {
  auto scoped_namespace = std::unique_ptr<ScopedNamespace>(new ScopedNamespace);
  PL_RETURN_IF_ERROR(scoped_namespace->EnterNamespace(ns_pid, ns_type));
  return scoped_namespace;
}

ScopedNamespace::~ScopedNamespace() { ExitNamespace(); }

Status ScopedNamespace::EnterNamespace(int ns_pid, std::string_view ns_type) {
  const auto& proc_path = system::Config::GetInstance().proc_path();

  std::filesystem::path orig_ns_path = proc_path / "self/ns" / ns_type;
  orig_ns_fd_ = open(orig_ns_path.string().c_str(), O_RDONLY);
  if (orig_ns_fd_ < 0) {
    return error::Internal("Could not access original namespace FD [path=$0]",
                           orig_ns_path.string());
  }

  std::filesystem::path ns_path = proc_path / std::to_string(ns_pid) / "ns" / ns_type;
  ns_fd_ = open(ns_path.string().c_str(), O_RDONLY);
  if (ns_fd_ < 0) {
    return error::Internal("Could not access target namespace FD [path=$0]", ns_path.string());
  }

  // Switch network namespaces, so socket prober connects to the target network namespace.
  setns_retval_ = setns(ns_fd_, 0);
  if (setns_retval_ != 0) {
    return error::Internal("Could not change to network namespace of PID $0 [err=$1]", ns_pid,
                           std::strerror(errno));
  }

  return Status::OK();
}

void ScopedNamespace::ExitNamespace() {
  // Process in reverse order of EnterNamespace.
  if (setns_retval_ != 0) {
    return;
  }
  ECHECK_EQ(setns(orig_ns_fd_, 0), 0) << "Uh-oh...could not restore original namespace.";

  if (ns_fd_ < 0) {
    return;
  }
  close(ns_fd_);

  if (orig_ns_fd_ < 0) {
    return;
  }
  close(orig_ns_fd_);
}

}  // namespace system
}  // namespace pl
