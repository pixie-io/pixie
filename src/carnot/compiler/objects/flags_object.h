#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace compiler {

class FlagsObject : public QLObject {
 public:
  static constexpr TypeDescriptor FlagsTypeDescriptor = {
      /* name */ "flags",
      /* type */ QLObjectType::kFlags,
  };

  inline static constexpr char kParseMethodName[] = "parse";

  static StatusOr<std::shared_ptr<FlagsObject>> Create() {
    auto obj = std::shared_ptr<FlagsObject>(new FlagsObject());
    PL_RETURN_IF_ERROR(obj->Init());
    return obj;
  }

 protected:
  FlagsObject() : QLObject(FlagsTypeDescriptor) {}
  Status Init();
  bool HasNonMethodAttribute(std::string_view name) const override;

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr& ast,
                                         std::string_view name) const override;
  StatusOr<QLObjectPtr> GetFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> DefineFlagHandler(const pypa::AstPtr& ast, const ParsedArgs& args);
  StatusOr<QLObjectPtr> ParseFlagsHandler(const pypa::AstPtr& ast, const ParsedArgs& args);

 private:
  // Whether or not .parse() has been called on this object.
  bool parsed_flags_ = false;
  absl::flat_hash_map<std::string, DataIR*> flag_values_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
