#include "src/carnot/planner/compiler/var_table.h"

namespace pl {
namespace carnot {
namespace compiler {

std::shared_ptr<VarTable> VarTable::Create() {
  // Constructor is hidden so must use raw ptr instantiation.
  return std::shared_ptr<VarTable>(new VarTable());
}

std::shared_ptr<VarTable> VarTable::Create(std::shared_ptr<VarTable> parent_scope) {
  // Constructor is hidden so must use raw ptr instantiation.
  return std::shared_ptr<VarTable>(new VarTable(parent_scope));
}

QLObjectPtr VarTable::Lookup(std::string_view name) {
  if (scope_table_.contains(name)) {
    return scope_table_[name];
  }
  if (parent_scope_ == nullptr) {
    return nullptr;
  }
  return parent_scope_->Lookup(name);
}

bool VarTable::HasVariable(std::string_view name) { return Lookup(name) != nullptr; }

void VarTable::Add(std::string_view name, QLObjectPtr ql_object) { scope_table_[name] = ql_object; }

std::shared_ptr<VarTable> VarTable::CreateChild() { return VarTable::Create(shared_from_this()); }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
