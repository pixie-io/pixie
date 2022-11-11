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

struct LabelSelectorSpec {
  LabelSelectorSpec(const absl::flat_hash_map<std::string, std::string>& labels,
                    const std::string& ns, const std::string& container_name,
                    const std::string& process)
      : labels_(labels), namespace_(ns), container_name_(container_name), process_(process) {}

  template <typename H>
  friend H AbslHashValue(H h, const LabelSelectorSpec& c) {
    return H::combine(std::move(h), c.labels_, c.namespace_, c.container_name_, c.process_);
  }

  bool operator==(const LabelSelectorSpec& rhs) const {
    return this->labels_ == rhs.labels_ && this->namespace_ == rhs.namespace_ &&
           this->container_name_ == rhs.container_name_ && this->process_ == rhs.process_;
  }
  bool operator!=(const LabelSelectorSpec& rhs) const { return !(*this == rhs); }

  absl::flat_hash_map<std::string, std::string> labels_;
  std::string namespace_;
  std::string container_name_;
  std::string process_;
};

class LabelSelectorTarget : public QLObject {
 public:
  static constexpr TypeDescriptor LabelSelectorTracepointType = {
      /* name */ "LabelSelectorTarget",
      /* type */ QLObjectType::kLabelSelectorTarget,
  };

  static StatusOr<std::shared_ptr<LabelSelectorTarget>> Create(
      const pypa::AstPtr& ast, ASTVisitor* visitor,
      const absl::flat_hash_map<std::string, std::string>& labels, const std::string& ns,
      const std::string& container_name, const std::string& cmdline) {
    return std::shared_ptr<LabelSelectorTarget>(
        new LabelSelectorTarget(ast, visitor, labels, ns, container_name, cmdline));
  }

  static bool IsLabelSelectorTarget(const QLObjectPtr& ptr) {
    return ptr->type() == LabelSelectorTracepointType.type();
  }

  LabelSelectorSpec target() const { return {labels_, namespace_, container_name_, process_}; }

 private:
  LabelSelectorTarget(const pypa::AstPtr& ast, ASTVisitor* visitor,
                      const absl::flat_hash_map<std::string, std::string>& labels,
                      const std::string& ns, const std::string& container_name,
                      const std::string& cmdline)
      : QLObject(LabelSelectorTracepointType, ast, visitor),
        labels_(labels),
        namespace_(ns),
        container_name_(container_name),
        process_(cmdline) {}

  absl::flat_hash_map<std::string, std::string> labels_;
  std::string namespace_;
  std::string container_name_;
  std::string process_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
