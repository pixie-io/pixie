#include "src/carnot/planner/objects/var_table.h"
#include "src/carnot/planner/objects/funcobject.h"

namespace pl {
namespace carnot {
namespace planner {
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

absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>> VarTable::GetVisFuncs() {
  DCHECK_EQ(parent_scope_, nullptr);
  absl::flat_hash_map<std::string, std::shared_ptr<FuncObject>> vis_funcs;
  for (const auto& [name, object] : scope_table_) {
    if (object->type() != QLObjectType::kFunction) {
      continue;
    }
    std::shared_ptr<FuncObject> func_object = std::static_pointer_cast<FuncObject>(object);
    // Only keep the func objects that have a visualization spec.
    if (!func_object->HasVisSpec()) {
      continue;
    }
    vis_funcs[name] = func_object;
  }
  return vis_funcs;
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
