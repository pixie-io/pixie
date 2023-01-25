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
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/rules/rules.h"

namespace px {
namespace carnot {
namespace planner {
/**
 * @brief Class to handle rulebatch execution strategies.
 * New strategies inherit from this class and must implement the
 * MaxIterations handler.
 */
class Strategy {
 public:
  virtual ~Strategy() = default;
  explicit Strategy(const std::string& name, int64_t max_iterations)
      : name_(name), max_iterations_(max_iterations) {}

  virtual Status MaxIterationsHandler() = 0;
  int64_t max_iterations() { return max_iterations_; }

 protected:
  std::string name_;

 private:
  int64_t max_iterations_;
};

/**
 * @brief Fail upon exceeding the set number of iterations.
 *
 */
class FailOnMax : public Strategy {
 public:
  FailOnMax(const std::string& name, int64_t max_iterations) : Strategy(name, max_iterations) {}
  Status MaxIterationsHandler() override {
    return error::DeadlineExceeded("Reached max iterations ($0) for rule batch '$1'",
                                   max_iterations(), name_);
  }
};

/**
 * @brief When max iterations are reached, don't fail and warn quietly.
 */
class TryUntilMax : public Strategy {
 public:
  TryUntilMax(const std::string& name, int64_t max_iterations) : Strategy(name, max_iterations) {}
  Status MaxIterationsHandler() override {
    VLOG(1) << absl::Substitute("Max iterations reached for rule batch $0. Continuing.", name_);
    return Status::OK();
  }
};

/**
 * @brief Run the rule batch once and silently accept that the max iterations are reached.
 */
class DoOnce : public Strategy {
 public:
  explicit DoOnce(const std::string& name) : Strategy(name, 1) {}
  Status MaxIterationsHandler() override { return Status::OK(); }
};

/**
 * @brief Rules modify the IR graph. Each rule should extend this
 * abstract class.
 */
template <typename TRuleType>
class BaseRuleBatch {
 public:
  BaseRuleBatch(std::string name, std::unique_ptr<Strategy> strategy)
      : name_(name), strategy_(std::move(strategy)) {}

  const std::vector<std::unique_ptr<TRuleType>>& rules() const { return rules_; }
  int64_t max_iterations() const { return strategy_->max_iterations(); }
  Status MaxIterationsHandler() const { return strategy_->MaxIterationsHandler(); }

  template <typename R, typename... Args>
  R* AddRule(Args... args) {
    std::unique_ptr<R> rule = std::make_unique<R>(args...);
    R* raw_rule = rule.get();
    rules_.push_back(std::move(rule));
    return raw_rule;
  }
  const std::string name() const { return name_; }

 private:
  std::string name_;
  std::unique_ptr<Strategy> strategy_;
  std::vector<std::unique_ptr<TRuleType>> rules_;
};

using RuleBatch = BaseRuleBatch<Rule>;

template <typename TPlan>
class RuleExecutor {
  using TRule = BaseRule<TPlan>;
  using TRuleBatch = BaseRuleBatch<BaseRule<TPlan>>;

 public:
  virtual ~RuleExecutor() = default;
  // TODO(philkuz) figure out how to collect stats on the execution.
  Status Execute(TPlan* ir_graph) {
    for (const auto& rb : rule_batches) {
      bool can_continue = true;
      int64_t iteration = 0;
      // We continue executing a batch until a stop condition is met.
      while (can_continue) {
        iteration += 1;
        bool graph_is_updated = false;
        for (const auto& rule : rb->rules()) {
          PX_ASSIGN_OR_RETURN(bool rule_updates_graph, rule->Execute(ir_graph));
          graph_is_updated = graph_is_updated || rule_updates_graph;
        }
        if (iteration >= rb->max_iterations() && graph_is_updated) {
          PX_RETURN_IF_ERROR(rb->MaxIterationsHandler());
          // TODO(philkuz) Reviewer: should this be a failure somehow?
          can_continue = false;
        }
        // TODO(philkuz) in the future we might have a fast way to check whether the graph has
        // changed. For now let's be lazy and leave it up to the user to make sure rules properly
        // update.
        // (graph_is_updated == false) => the graph has reached a fixed point and is done
        if (!graph_is_updated) {
          can_continue = false;
        }
      }
    }
    return Status::OK();
  }
  template <typename S, typename... Args>
  TRuleBatch* CreateRuleBatch(std::string name, Args... args) {
    std::unique_ptr<TRuleBatch> rb(new TRuleBatch(name, std::make_unique<S>(name, args...)));
    TRuleBatch* out_ptr = rb.get();
    rule_batches.push_back(std::move(rb));
    return out_ptr;
  }

 private:
  std::vector<std::unique_ptr<TRuleBatch>> rule_batches;
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
