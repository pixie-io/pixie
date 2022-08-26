/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>

#include <absl/strings/substitute.h>
#include "src/shared/metadata/k8s_objects.h"

namespace px {
namespace md {

std::string PodInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Pod:ns=$1:name=$2:uid=$3:state=$4:start=$5:stop=$6>", Indent(indent),
                          ns(), name(), uid(), state, start_time_ns(), stop_time_ns());
}

std::string ContainerInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Container:cid=$1:name=$2:pod_id=$3:state=$4:start=$5:stop=$6>",
                          Indent(indent), cid(), name(), pod_id(), state, start_time_ns(),
                          stop_time_ns());
}

std::string ServiceInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Service:ns=$1:name=$2:uid=$3:state=$4:start=$5:stop=$6>",
                          Indent(indent), ns(), name(), uid(), state, start_time_ns(),
                          stop_time_ns());
}

std::string NamespaceInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Namespace:ns=$1:name=$2:uid=$3:state=$4:start=$5:stop=$6>",
                          Indent(indent), ns(), name(), uid(), state, start_time_ns(),
                          stop_time_ns());
}

std::string ReplicaSetInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<ReplicaSet:ns=$1:name=$2:uid=$3:state=$4:start=$5:stop=$6>",
                          Indent(indent), ns(), name(), uid(), state, start_time_ns(),
                          stop_time_ns());
}

std::string DeploymentInfo::DebugString(int indent) const {
  std::string state = stop_time_ns() != 0 ? "S" : "R";
  return absl::Substitute("$0<Deployment:ns=$1:name=$2:uid=$3:state=$4:start=$5:stop=$6>",
                          Indent(indent), ns(), name(), uid(), state, start_time_ns(),
                          stop_time_ns());
}

}  // namespace md
}  // namespace px
