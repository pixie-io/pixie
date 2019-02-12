#include "src/carnot/compiler/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

Status IR::AddEdge(int64_t from_node, int64_t to_node) {
  dag_.AddEdge(from_node, to_node);
  return Status::OK();
}

Status IR::AddEdge(IRNode* from_node, IRNode* to_node) {
  return AddEdge(from_node->id(), to_node->id());
}

std::string IR::DebugString() {
  std::string debug_string;
  debug_string += dag().DebugString() + "\n";
  for (auto& a : nodes_) {
    debug_string += a->DebugString(0) + "\n";
  }
  return debug_string;
}

void IRNode::SetLineCol(int64_t line, int64_t col) {
  line_ = line;
  col_ = col;
}

bool MemorySourceIR::HasLogicalRepr() const { return true; }

Status MemorySourceIR::Init(IRNode* table_node, IRNode* select) {
  table_node_ = table_node;
  select_ = select;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, select_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, table_node_));

  return Status::OK();
}

std::string debugString(int64_t depth, std::string name,
                        std::map<std::string, std::string> property_value_map) {
  std::vector<std::string> property_strings;
  std::map<std::string, std::string>::iterator it;
  std::string depth_string = std::string(depth, '\t');
  property_strings.push_back(absl::StrFormat("%s%s", depth_string, name));

  for (it = property_value_map.begin(); it != property_value_map.end(); it++) {
    std::string prop_str = absl::Substitute("$0 $1\t-$2", depth_string, it->first, it->second);
    property_strings.push_back(prop_str);
  }
  return absl::StrJoin(property_strings, "\n");
}
std::string MemorySourceIR::DebugString(int64_t depth) const {
  return debugString(
      depth, absl::StrFormat("%d:MemorySourceIR", id()),
      {{"From", table_node_->DebugString(depth + 1)}, {"Select", select_->DebugString(depth + 1)}});
}

Status RangeIR::Init(IRNode* parent, IRNode* time_repr) {
  // TODO(philkuz) implement string to ms (int) conversion.
  time_repr_ = time_repr;
  parent_ = parent;
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, time_repr_));
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(parent_, this));
  return Status::OK();
}

bool RangeIR::HasLogicalRepr() const { return false; }

std::string RangeIR::DebugString(int64_t depth) const {
  return debugString(
      depth, absl::StrFormat("%d:RangeIR", id()),
      {{"Parent", parent_->DebugString(depth + 1)}, {"Time", time_repr_->DebugString(depth + 1)}});
}

bool StringIR::HasLogicalRepr() const { return false; }
Status StringIR::Init(std::string str) {
  str_ = str;
  return Status();
}

std::string StringIR::DebugString(int64_t depth) const {
  return absl::StrFormat("%s%d:%s\t-\t%s", std::string(depth, '\t'), id(), "Str", str());
}

bool ListIR::HasLogicalRepr() const { return false; }

Status ListIR::AddListItem(IRNode* node) {
  children_.push_back(node);
  PL_RETURN_IF_ERROR(graph_ptr()->AddEdge(this, node));
  return Status::OK();
}
std::string ListIR::DebugString(int64_t depth) const {
  std::map<std::string, std::string> childMap;
  for (size_t i = 0; i < children_.size(); i++) {
    childMap[absl::StrFormat("child%d", i)] = children_[i]->DebugString(depth + 1);
  }
  return debugString(depth, absl::StrFormat("%d:ListIR", id()), childMap);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
