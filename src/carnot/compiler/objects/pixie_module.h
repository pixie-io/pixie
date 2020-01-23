#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/compiler_state/compiler_state.h"
#include "src/carnot/compiler/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace compiler {

class PixieModule : public QLObject {
 public:
  static constexpr TypeDescriptor PixieModuleType = {
      /* name */ "pl",
      /* type */ QLObjectType::kPLModule,
  };
  static StatusOr<std::shared_ptr<PixieModule>> Create(IR* graph, CompilerState* compiler_state);

  // Constant for the modules.
  inline static constexpr char kPixieModuleObjName[] = "pl";

  // Constants for operators in the query language.
  inline static constexpr char kDataframeOpId[] = "DataFrame";
  inline static constexpr char kDisplayOpId[] = "display";
  inline static constexpr char kNowOpId[] = "now";
  inline static constexpr char kUInt128ConversionId[] = "uint128";
  static const constexpr char* const kTimeFuncs[] = {"minutes", "hours",        "seconds",
                                                     "days",    "microseconds", "milliseconds"};

 protected:
  explicit PixieModule(IR* graph, CompilerState* compiler_state)
      : QLObject(PixieModuleType), graph_(graph), compiler_state_(compiler_state) {}
  Status Init();
  Status RegisterUDFFuncs();
  Status RegisterUDTFs();
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
}  // namespace carnot
}  // namespace pl
