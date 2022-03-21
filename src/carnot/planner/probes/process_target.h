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

#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/shared/upid/upid.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief ProcessSpec is a structure used to specify a process in the cluster.
 * Some processes can simplify be specified by their pod name if there's only
 * one container and process for that pod. However, if there's > 1 container you need
 * to specify a container. If there's more than one process per the selected container, you
 * need to specify the process.
 *
 * You can also specify the pod and process if there is one container but multiple
 */
struct ProcessSpec {
  ProcessSpec(const std::string& pod_name, const std::string& container_name,
              const std::string& process)
      : pod_name_(pod_name), container_name_(container_name), process_(process) {}

  template <typename H>
  friend H AbslHashValue(H h, const ProcessSpec& c) {
    return H::combine(std::move(h), c.pod_name_, c.container_name_, c.process_);
  }

  bool operator==(const ProcessSpec& rhs) const {
    return this->pod_name_ == rhs.pod_name_ && this->container_name_ == rhs.container_name_ &&
           this->process_ == rhs.process_;
  }
  bool operator!=(const ProcessSpec& rhs) const { return !(*this == rhs); }

  std::string pod_name_;
  std::string container_name_;
  std::string process_;
};

/**
 * @brief ProcessTarget is the QLObject that wraps a process specification used as a target for
 * tracepoint deployments.
 *
 */
class ProcessTarget : public QLObject {
 public:
  static constexpr TypeDescriptor ProcessTracepointType = {
      /* name */ "ProcessTarget",
      /* type */ QLObjectType::kProcessTarget,
  };

  static StatusOr<std::shared_ptr<ProcessTarget>> Create(const pypa::AstPtr& ast,
                                                         ASTVisitor* visitor,
                                                         const std::string& pod_name,
                                                         const std::string& container_name,
                                                         const std::string& cmdline) {
    return std::shared_ptr<ProcessTarget>(
        new ProcessTarget(ast, visitor, pod_name, container_name, cmdline));
  }

  static bool IsProcessTarget(const QLObjectPtr& ptr) {
    return ptr->type() == ProcessTracepointType.type();
  }

  ProcessSpec target() const { return {pod_name_, container_name_, process_}; }

 private:
  ProcessTarget(const pypa::AstPtr& ast, ASTVisitor* visitor, const std::string& pod_name,
                const std::string& container_name, const std::string& cmdline)
      : QLObject(ProcessTracepointType, ast, visitor),
        pod_name_(pod_name),
        container_name_(container_name),
        process_(cmdline) {}
  std::string pod_name_;
  std::string container_name_;
  std::string process_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
