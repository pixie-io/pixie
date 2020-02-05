#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/objects/qlobject.h"
#include "src/carnot/compiler/plannerpb/query_flags.pb.h"

namespace pl {
namespace carnot {
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

  static StatusOr<std::shared_ptr<FlagsObject>> Create(IR* ir_graph, const FlagValues& values) {
    auto obj = std::shared_ptr<FlagsObject>(new FlagsObject(ir_graph));
    PL_RETURN_IF_ERROR(obj->Init(values));
    return obj;
  }

 protected:
  explicit FlagsObject(IR* ir_graph) : QLObject(FlagsTypeDescriptor), ir_graph_(ir_graph) {}

  Status Init(const FlagValues& values);
  bool HasNonMethodAttribute(std::string_view name) const override;

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr& ast,
                                         std::string_view name) const override;
  StatusOr<QLObjectPtr> GetFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> DefineFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> ParseFlagsHandler(const pypa::AstPtr& ast, const ParsedArgs& args);

 private:
  IR* ir_graph_;
  // Whether or not .parse() has been called on this object.
  bool parsed_flags_ = false;
  // Flags that have been registered by a call to px.flags.register.
  absl::flat_hash_map<std::string, DataIR*> registered_flag_values_;
  // Flags that were passed into the QueryRequest via 'flag_values'.
  absl::flat_hash_map<std::string, DataIR*> input_flag_values_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
