#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace compiler {

class PLModule : public QLObject {
 public:
  static constexpr TypeDescriptor PLModuleType = {
      /* name */ "pl",
      /* type */ QLObjectType::kPLModule,
  };
  static StatusOr<std::shared_ptr<PLModule>> Create(IR* graph, CompilerState* compiler_state);

  // Constants for operators in the query language.
  inline static constexpr char kDataframeOpId[] = "DataFrame";
  inline static constexpr char kDisplayOpId[] = "display";
  inline static constexpr char kNowOpId[] = "now";
  static const constexpr char* const kTimeFuncs[] = {"minutes", "hours",        "seconds",
                                                     "days",    "microseconds", "milliseconds"};

 protected:
  explicit PLModule(IR* graph, CompilerState* compiler_state)
      : QLObject(PLModuleType), graph_(graph), compiler_state_(compiler_state) {}
  Status Init();
  Status RegisterUDFFuncs();
  Status RegisterCompileTimeFuncs();
  Status RegisterCompileTimeUnitFunction(std::string name);

  StatusOr<std::shared_ptr<QLObject>> GetAttributeImpl(const pypa::AstPtr& ast,
                                                       std::string_view name) const override;

 private:
  IR* graph_;
  CompilerState* compiler_state_;
  absl::flat_hash_set<std::string> compiler_time_fns_;
};

/**
 * @brief Implements the pl.DataFrame() logic.
 *
 */
class DataFrameHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args);
};

/**
 * @brief Implements the pl.display() logic.
 *
 */
class DisplayHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args);
};

/**
 * @brief Implements the pl.now() and pl.minutes,pl.hours, etc.
 *
 */
class CompileTimeFuncHandler {
 public:
  static StatusOr<QLObjectPtr> NowEval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args);
  static StatusOr<QLObjectPtr> TimeEval(IR* graph, std::string name, const pypa::AstPtr& ast,
                                        const ParsedArgs& args);
};

/**
 * @brief Implements the udf logic.
 *
 */
class UDFHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, std::string name, const pypa::AstPtr& ast,
                                    const ParsedArgs& args);
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
