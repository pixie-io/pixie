#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/qlobject.h"
#include "src/carnot/planner/objects/type_object.h"
#include "src/carnot/planner/plannerpb/query_flags.pb.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

using FlagValue = plannerpb::QueryRequest::FlagValue;
using FlagValues = std::vector<FlagValue>;

class FlagsObject : public QLObject {
 public:
  static constexpr TypeDescriptor FlagsTypeDescriptor = {
      /* name */ "flags",
      /* type */ QLObjectType::kFlags,
  };

  inline static constexpr char kParseMethodName[] = "parse";

  static StatusOr<std::shared_ptr<FlagsObject>> Create(IR* ir_graph, const FlagValues& values,
                                                       ASTVisitor* visitor) {
    auto obj = std::shared_ptr<FlagsObject>(
        new FlagsObject(ir_graph, /*default_zero_values*/ false, visitor));
    PL_RETURN_IF_ERROR(obj->Init(values));
    return obj;
  }

  static StatusOr<std::shared_ptr<FlagsObject>> CreateParseOnly(IR* ir_graph,
                                                                ASTVisitor* ast_visitor) {
    auto obj = std::shared_ptr<FlagsObject>(
        new FlagsObject(ir_graph, /*default_zero_values*/ true, ast_visitor));
    PL_RETURN_IF_ERROR(obj->Init(/*flag_values*/ {}));
    return obj;
  }

  StatusOr<plannerpb::QueryFlagsSpec> GetAvailableFlags(const pypa::AstPtr& ast) const;

 protected:
  explicit FlagsObject(IR* ir_graph, bool default_zero_values, ASTVisitor* ast_visitor)
      : QLObject(FlagsTypeDescriptor, ast_visitor),
        ir_graph_(ir_graph),
        default_zero_values_(default_zero_values) {}

  Status Init(const FlagValues& values);
  bool HasNonMethodAttribute(std::string_view name) const override;
  bool HasFlag(std::string_view flag_name) const;

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr& ast,
                                         std::string_view name) const override;
  StatusOr<QLObjectPtr> GetFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> DefineFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> ParseFlagsHandler(const pypa::AstPtr& ast, const ParsedArgs& args);

 private:
  IR* ir_graph_;
  // Whether or not .parse() has been called on this object.
  bool parsed_flags_ = false;
  // Whether or not to emit the zero value for a given flag when there is no input value for that
  // flag. Used when GetAvailableFlags is called, because there may be flags that are required in
  // the script which we need to successfully parse that are not known or needed for
  // GetAvailableFlags.
  bool default_zero_values_ = false;
  // Flags that were passed into the QueryRequest via 'flag_values'.
  absl::flat_hash_map<std::string, DataIR*> input_flag_values_;
  // Flags that have been registered by a call to px.flags.register.
  absl::flat_hash_map<std::string, DataIR*> default_flag_values_;
  // The data type of the flag that is registered.
  // TODO(philkuz, nserrino): Add registered_semantic_types_ or make TypeObject extensible
  // to semantic types.
  absl::flat_hash_map<std::string, std::shared_ptr<TypeObject>> flag_types_;
  // Stored as a map to make it easy to output flags in alphabetic order in GetAvailableFlags.
  std::map<std::string, std::string> flag_descriptions_;
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
