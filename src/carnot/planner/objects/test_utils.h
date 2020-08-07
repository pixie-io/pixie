#pragma once

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/planner/compiler/ast_visitor.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/objects/collection_object.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/dict_object.h"
#include "src/carnot/planner/objects/expr_object.h"
#include "src/carnot/planner/objects/none_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

constexpr char kRegistryInfoProto[] = R"proto(
udtfs {
  name: "GetUDAList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "return_type"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
udtfs {
  name: "GetUDFList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "return_type"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
udtfs {
  name: "GetUDTFList"
  executor: UDTF_ONE_KELVIN
  relation {
    columns {
      column_name: "name"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "executor"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "init_args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "output_relation"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
)proto";

class QLObjectTest : public OperatorTests {
 protected:
  void SetUp() override {
    OperatorTests::SetUp();

    info = std::make_shared<RegistryInfo>();
    udfspb::UDFInfo info_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(kRegistryInfoProto, &info_pb));
    PL_CHECK_OK(info->Init(info_pb));
    compiler_state =
        std::make_shared<CompilerState>(std::make_unique<RelationMap>(), info.get(), 0);
    // Graph is set in OperatorTests.

    ast_visitor =
        ASTVisitorImpl::Create(graph.get(), &dynamic_trace_, compiler_state.get(), &module_handler)
            .ConsumeValueOrDie();
  }

  ArgMap MakeArgMap(const std::vector<std::pair<std::string, IRNode*>>& kwargs,
                    const std::vector<IRNode*>& args) {
    std::vector<NameToNode> converted_kwargs;
    std::vector<QLObjectPtr> converted_args;
    for (const auto& p : kwargs) {
      converted_kwargs.push_back(
          {p.first, QLObject::FromIRNode(p.second, ast_visitor.get()).ConsumeValueOrDie()});
    }
    for (IRNode* node : args) {
      converted_args.push_back(QLObject::FromIRNode(node, ast_visitor.get()).ConsumeValueOrDie());
    }
    return ArgMap{converted_kwargs, converted_args};
  }

  QLObjectPtr ToQLObject(IRNode* node) {
    return QLObject::FromIRNode(node, ast_visitor.get()).ConsumeValueOrDie();
  }

  template <typename... Args>
  std::shared_ptr<ListObject> MakeListObj(Args... nodes) {
    std::vector<QLObjectPtr> objs;
    for (const auto node : std::vector<IRNode*>{nodes...}) {
      objs.push_back(ToQLObject(node));
    }
    return ListObject::Create(objs, ast_visitor.get()).ConsumeValueOrDie();
  }

  template <typename... Args>
  std::shared_ptr<TupleObject> MakeTupleObj(Args... nodes) {
    std::vector<QLObjectPtr> objs;
    for (const auto node : std::vector<IRNode*>{nodes...}) {
      objs.push_back(ToQLObject(node));
    }
    return TupleObject::Create(objs, ast_visitor.get()).ConsumeValueOrDie();
  }

  std::shared_ptr<CompilerState> compiler_state = nullptr;
  std::shared_ptr<RegistryInfo> info = nullptr;
  std::shared_ptr<ASTVisitor> ast_visitor = nullptr;
  ModuleHandler module_handler;
  MutationsIR dynamic_trace_;
};

StatusOr<QLObjectPtr> NoneObjectFunc(const pypa::AstPtr&, const ParsedArgs&, ASTVisitor* visitor) {
  return StatusOr<QLObjectPtr>(std::make_shared<NoneObject>(visitor));
}

std::string PrintObj(QLObjectPtr obj) {
  switch (obj->type()) {
    case QLObjectType::kList: {
      auto list = std::static_pointer_cast<CollectionObject>(obj);
      std::vector<std::string> items;
      for (auto item : list->items()) {
        items.push_back(PrintObj(item));
      }
      return absl::Substitute("[$0]", absl::StrJoin(items, ","));
    }
    case QLObjectType::kDict: {
      auto dict = std::static_pointer_cast<DictObject>(obj);
      std::vector<std::string> items;
      for (const auto& [i, key] : Enumerate(dict->keys())) {
        auto value = dict->values()[i];
        items.push_back(absl::Substitute("'$0', $1", PrintObj(key), PrintObj(value)));
      }
      return absl::Substitute("[$0]", absl::StrJoin(items, ","));
    }
    default:
      return std::string(obj->type_descriptor().name());
  }
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
