#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/flags_object.h"
#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/plannerpb/query_flags.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

class PixieModule : public QLObject {
 public:
  static constexpr TypeDescriptor PixieModuleType = {
      /* name */ "px",
      /* type */ QLObjectType::kPLModule,
  };
  static StatusOr<std::shared_ptr<PixieModule>> Create(IR* graph, CompilerState* compiler_state,
                                                       const FlagValues& flag_values);

  // Constant for the modules.
  inline static constexpr char kPixieModuleObjName[] = "px";
  inline static constexpr char kOldPixieModuleObjName[] = "pl";

  // Constants for operators in the query language.
  inline static constexpr char kDataframeOpId[] = "DataFrame";
  inline static constexpr char kDisplayOpId[] = "display";
  inline static constexpr char kFlagsOpId[] = "flags";
  inline static constexpr char kNowOpId[] = "now";
  inline static constexpr char kUInt128ConversionId[] = "uint128";
  static const constexpr char* const kTimeFuncs[] = {"minutes", "hours",        "seconds",
                                                     "days",    "microseconds", "milliseconds"};
  std::shared_ptr<FlagsObject> flags_object() { return flags_object_; }

 protected:
  explicit PixieModule(IR* graph, CompilerState* compiler_state)
      : QLObject(PixieModuleType), graph_(graph), compiler_state_(compiler_state) {}
  Status Init(const FlagValues& flag_values);
  Status RegisterFlags(const FlagValues& flag_values);
  Status RegisterUDFFuncs();
  Status RegisterUDTFs();
  Status RegisterCompileTimeFuncs();
  Status RegisterCompileTimeUnitFunction(std::string name);

 private:
  IR* graph_;
  CompilerState* compiler_state_;
  absl::flat_hash_set<std::string> compiler_time_fns_;
  // Keep a handle on flags_object separate from attributes in case it gets reassigned.
  std::shared_ptr<FlagsObject> flags_object_;
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
 * @brief Implements the pl.now(), pl.minutes(), pl.hours(), etc.
 *
 */
class CompileTimeFuncHandler {
 public:
  static StatusOr<QLObjectPtr> NowEval(IR* graph, const pypa::AstPtr& ast, const ParsedArgs& args);
  static StatusOr<QLObjectPtr> TimeEval(IR* graph, std::string name, const pypa::AstPtr& ast,
                                        const ParsedArgs& args);
  static StatusOr<QLObjectPtr> UInt128Conversion(IR* graph, const pypa::AstPtr& ast,
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

/**
 * @brief Implements the logic that implements udtf_source_specification.
 *
 */
class UDTFSourceHandler {
 public:
  static StatusOr<QLObjectPtr> Eval(IR* graph, const udfspb::UDTFSourceSpec& udtf_source_spec,
                                    const pypa::AstPtr& ast, const ParsedArgs& args);

 private:
  static StatusOr<ExpressionIR*> EvaluateExpression(IR* graph, IRNode* arg_node,
                                                    const udfspb::UDTFSourceSpec::Arg& arg);
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
