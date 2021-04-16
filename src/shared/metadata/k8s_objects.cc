#include <string>

#include <absl/strings/substitute.h>
#include "src/shared/metadata/k8s_objects.h"

namespace px {
namespace md {

std::string PodInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Pod:ns=$1:name=$2:uid=$3:state=$4>", Indent(indent), ns(), name(),
                          uid(), state);
}

std::string ContainerInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Container:cid=$1:name=$2:pod_id=$3:state=$4>", Indent(indent), cid(),
                          name(), pod_id(), state);
}

std::string ServiceInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Service:ns=$1:name=$2:uid=$3:state=$4>", Indent(indent), ns(), name(),
                          uid(), state);
}

std::string NamespaceInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Namespace:ns=$1:name=$2:uid=$3:state=$4>", Indent(indent), ns(),
                          name(), uid(), state);
}

}  // namespace md
}  // namespace px
