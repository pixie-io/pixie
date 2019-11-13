#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/ir/ir_nodes.h"
#include "src/carnot/compiler/rules.h"

namespace pl {
namespace carnot {
namespace compiler {
/**
 * @brief Class to handle rulebatch execution strategies.
 * New strategies inherit from this class and must implement the
 * MaxIterations handler.
 */
class Strategy {
 public:
  virtual ~Strategy() = default;
  explicit Strategy(int64_t max_iterations) : max_iterations_(max_iterations) {}

  virtual Status MaxIterationsHandler() = 0;
  int64_t max_iterations() { return max_iterations_; }

 private:
  int64_t max_iterations_;
};

/**
 * @brief Fail upon exceeding the set number of iterations.
 *
 */
class FailOnMax : public Strategy {
 public:
  explicit FailOnMax(int64_t max_iterations) : Strategy(max_iterations) {}
  Status MaxIterationsHandler() override {
    return error::DeadlineExceeded("Max iterations reached.");
  }
};

/**
 * @brief When max iterations are reached, don't fail and warn quietly.
 */
class TryUntilMax : public Strategy {
 public:
  explicit TryUntilMax(int64_t max_iterations) : Strategy(max_iterations) {}
  Status MaxIterationsHandler() override {
    LOG(WARNING) << "Max iterations reached. Continuing.";
    return Status::OK();
  }
};

/**
 * @brief Run the rule batch once and silently accept that the max iterations are reached.
 */
class DoOnce : public Strategy {
 public:
  DoOnce() : Strategy(1) {}
  Status MaxIterationsHandler() override { return Status::OK(); }
};

/**
 * @brief Rules modify the IR graph. Each rule should extend this
 * abstract class.
 */
class RuleBatch {
 public:
  RuleBatch(std::string name, std::unique_ptr<Strategy> strategy)
      : name_(name), strategy_(std::move(strategy)) {}

  const std::vector<std::unique_ptr<Rule>>& rules() const { return rules_; }
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
  std::vector<std::unique_ptr<Rule>> rules_;
};

class RuleExecutor {
 public:
  virtual ~RuleExecutor() = default;
  // TODO(philkuz) figure out how to collect stats on the execution.
  Status Execute(IR* ir_graph) {
    for (const auto& rb : rule_batches) {
      bool can_continue = true;
      int64_t iteration = 0;
      // We continue executing a batch until a stop condition is met.
      while (can_continue) {
        iteration += 1;
        bool graph_is_updated = false;
        for (const std::unique_ptr<Rule>& rule : rb->rules()) {
          PL_ASSIGN_OR_RETURN(bool rule_updates_graph, rule->Execute(ir_graph));
          graph_is_updated = graph_is_updated || rule_updates_graph;
        }
        if (iteration >= rb->max_iterations() && graph_is_updated) {
          PL_RETURN_IF_ERROR(rb->MaxIterationsHandler());
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
  RuleBatch* CreateRuleBatch(std::string name, Args... args) {
    std::unique_ptr<RuleBatch> rb(new RuleBatch(name, std::make_unique<S>(args...)));
    RuleBatch* out_ptr = rb.get();
    rule_batches.push_back(std::move(rb));
    return out_ptr;
  }

 private:
  std::vector<std::unique_ptr<RuleBatch>> rule_batches;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
