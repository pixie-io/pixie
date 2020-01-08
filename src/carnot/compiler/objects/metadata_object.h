#pragma once
#include <memory>
#include <string>

#include "src/carnot/compiler/objects/funcobject.h"
#include "src/carnot/compiler/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace compiler {

class MetadataObject : public QLObject {
 public:
  static constexpr TypeDescriptor MetadataTypeDescriptor = {
      /* name */ "metadata",
      /* type */ QLObjectType::kMetadata,
  };
  static StatusOr<std::shared_ptr<MetadataObject>> Create(OperatorIR* op);

 protected:
  explicit MetadataObject(OperatorIR* op) : QLObject(MetadataTypeDescriptor), op_(op) {}
  Status Init();

  StatusOr<QLObjectPtr> SubscriptHandler(const pypa::AstPtr& ast, const ParsedArgs& args);

  OperatorIR* op() const { return op_; }

 private:
  OperatorIR* op_;
};

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
