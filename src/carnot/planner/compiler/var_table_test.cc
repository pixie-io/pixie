#include <utility>

#include "src/carnot/planner/compiler/var_table.h"
#include "src/carnot/planner/objects/dataframe.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using VarTableTest = QLObjectTest;

TEST_F(VarTableTest, test_parent_var_table) {
  auto var_table = VarTable::Create();
  std::string var_name = "foo";
  EXPECT_FALSE(var_table->HasVariable(var_name));
  auto mem_src = MakeMemSource();

  auto dataframe_object = Dataframe::Create(mem_src, ast_visitor.get()).ConsumeValueOrDie();
  var_table->Add(var_name, dataframe_object);

  EXPECT_TRUE(var_table->HasVariable(var_name));
  EXPECT_EQ(var_table->Lookup(var_name), dataframe_object);
}

TEST_F(VarTableTest, test_nested_var_table_lookup) {
  auto parent_table = VarTable::Create();
  auto child_table = parent_table->CreateChild();

  std::string foo = "foo";
  EXPECT_FALSE(parent_table->HasVariable(foo));
  EXPECT_FALSE(child_table->HasVariable(foo));
  auto mem_src = MakeMemSource();

  auto dataframe_object = Dataframe::Create(mem_src, ast_visitor.get()).ConsumeValueOrDie();
  parent_table->Add(foo, dataframe_object);

  EXPECT_TRUE(parent_table->HasVariable(foo));
  EXPECT_TRUE(child_table->HasVariable(foo));
  EXPECT_EQ(parent_table->Lookup(foo), dataframe_object);
  EXPECT_EQ(child_table->Lookup(foo), dataframe_object);

  // Child table doesn't leak into parent.
  std::string bar = "bar";
  EXPECT_FALSE(parent_table->HasVariable(bar));
  EXPECT_FALSE(child_table->HasVariable(bar));

  auto dataframe_object2 = Dataframe::Create(mem_src, ast_visitor.get()).ConsumeValueOrDie();
  child_table->Add(bar, dataframe_object2);

  EXPECT_FALSE(parent_table->HasVariable(bar));
  EXPECT_TRUE(child_table->HasVariable(bar));
  EXPECT_EQ(child_table->Lookup(bar), dataframe_object2);
}

// Test to get viz funcs from the var table.
TEST_F(VarTableTest, test_viz_funcs) {
  auto var_table = VarTable::Create();

  // Add a non-func in.
  auto mem_src = MakeMemSource();
  auto dataframe_object = Dataframe::Create(mem_src, ast_visitor.get()).ConsumeValueOrDie();
  var_table->Add("foo", dataframe_object);

  // Add a func with no viz spec.
  std::shared_ptr<FuncObject> func_obj_no_viz =
      FuncObject::Create("no_viz", {}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&NoneObjectFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  var_table->Add("no_viz", func_obj_no_viz);

  std::string vega_spec = "aaaaa";
  // Add a func with a viz.
  std::shared_ptr<FuncObject> func_obj_with_viz =
      FuncObject::Create("with_viz", {}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&NoneObjectFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();
  auto viz_spec = std::make_unique<VizSpec>();
  viz_spec->vega_spec = vega_spec;
  EXPECT_OK(func_obj_with_viz->AddVizSpec(std::move(viz_spec)));
  var_table->Add("with_viz", func_obj_with_viz);

  auto viz_funcs = var_table->GetVizFuncs();
  EXPECT_EQ(viz_funcs.size(), 1);
  ASSERT_TRUE(viz_funcs.contains("with_viz"));
  ASSERT_TRUE(viz_funcs.find("with_viz")->second->HasVizSpec());
  EXPECT_EQ(viz_funcs.find("with_viz")->second->viz_spec()->vega_spec, vega_spec);
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
