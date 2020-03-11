#include <memory>
#include <string>

#include "src/carnot/planner/objects/funcobject.h"
#include "src/carnot/planner/objects/none_object.h"
#include "src/carnot/planner/objects/test_utils.h"
#include "src/carnot/planner/objects/viz_object.h"

namespace pl {
namespace carnot {
namespace planner {
namespace compiler {

using VizObjectTest = QLObjectTest;

StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&, ASTVisitor* visitor) {
  return StatusOr<QLObjectPtr>(std::make_shared<NoneObject>(visitor));
}

TEST_F(VizObjectTest, SubscriptWithString) {
  std::shared_ptr<FuncObject> func_obj =
      FuncObject::Create("func", {"fn"}, {}, /* has_variable_len_args */ false,
                         /* has_variable_len_kwargs */ false,
                         std::bind(&SimpleFunc, std::placeholders::_1, std::placeholders::_2,
                                   std::placeholders::_3),
                         ast_visitor.get())
          .ConsumeValueOrDie();

  auto viz_object_or_s = VisualizationObject::Create(ast_visitor.get());
  ASSERT_OK(viz_object_or_s);

  auto viz_object = viz_object_or_s.ConsumeValueOrDie();
  auto vega_fn_or_s = viz_object->GetMethod(VisualizationObject::kVegaAttrId);
  ASSERT_OK(vega_fn_or_s);

  auto vega_fn = vega_fn_or_s.ConsumeValueOrDie();
  auto decorator_fn_or_s = vega_fn->Call({{}, {ToQLObject(MakeString("abcd"))}}, ast);
  ASSERT_OK(decorator_fn_or_s);

  auto decorator_fn = GetCallMethod(ast, decorator_fn_or_s.ConsumeValueOrDie()).ConsumeValueOrDie();
  auto wrapped_fn_or_s = decorator_fn->Call({{}, {func_obj}}, ast);
  ASSERT_OK(wrapped_fn_or_s);

  auto call_or_s = GetCallMethod(ast, wrapped_fn_or_s.ConsumeValueOrDie());
  ASSERT_OK(call_or_s);

  auto call = call_or_s.ConsumeValueOrDie();
  ASSERT_TRUE(call->HasVizSpec());
  EXPECT_EQ(call->viz_spec().vega_spec, "abcd");
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace pl
