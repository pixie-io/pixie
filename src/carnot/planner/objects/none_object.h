#pragma once
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <pypa/ast/ast.hh>

#include "src/carnot/planner/objects/qlobject.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

/**
 * @brief NoneObject represents None in python, the "null" object. This is used as a proxy for void
 * return type in Python interpretation.
 *
 */
class NoneObject : public QLObject {
 public:
  static constexpr TypeDescriptor NoneType = {
      /* name */ "None",
      /* type */ QLObjectType::kNone,
  };

  /**
   * @brief Construct a None object that represents the null value in Python.
   *
   * @param ast the ast ptr for the
   */
  explicit NoneObject(pypa::AstPtr ast) : QLObject(NoneType, ast) {}
  NoneObject() : QLObject(NoneType) {}
  bool CanAssignAttribute(std::string_view /*attr_name*/) const override { return false; }
};

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
