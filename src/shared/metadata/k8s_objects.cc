#include <string>

#include "absl/strings/substitute.h"
#include "src/shared/metadata/k8s_objects.h"

namespace pl {
namespace md {

std::string PodInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Pod:ns=$1:name=$2:uid=$3:state=$4>", Indent(indent), ns(), name(),
                          uid(), state);
}

std::string ContainerInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Container:cid=$1:pod_id=$2:state=$3>", Indent(indent), cid(),
                          pod_id(), state);
}

}  // namespace md
}  // namespace pl
