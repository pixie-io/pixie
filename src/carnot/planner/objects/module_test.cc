#include <gtest/gtest.h>
#include <memory>

#include <absl/container/flat_hash_map.h>

#include "src/carnot/planner/objects/module.h"
#include "src/carnot/planner/objects/test_utils.h"

namespace px {
namespace carnot {
namespace planner {
namespace compiler {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

using ModuleTest = QLObjectTest;

constexpr char kModulePxl[] = R"pxl(
import px
def funcs():
    '''
    Merge the func helpers together
    '''
    udf = px.GetUDFList()
    uda = px.GetUDAList()
    udtf = px.GetUDTFList()
    udf.func_type = "UDF"
    uda.func_type = "UDA"
    udtf.func_type = "UDTF"
    udf_uda = udf.append(uda)
    udf_uda.output_relation = udf_uda.return_type
    udf_uda.executor = 'N/A'
    udf_uda.init_args = udf_uda.args
    return udf_uda.drop(['return_type', 'args']).append(udtf)

free_var = "imfree"
)pxl";

TEST_F(ModuleTest, load_and_use_modules) {
  auto module_or_s = Module::Create(kModulePxl, ast_visitor.get());
  ASSERT_OK(module_or_s);
  std::shared_ptr<Module> module = module_or_s.ConsumeValueOrDie();

  // Get "funcs" as an attribute.
  auto get_funcs_attr_status = module->GetAttribute(ast, "funcs");
  ASSERT_OK(get_funcs_attr_status);
  // Make sure it can be returned as a function.
  EXPECT_OK(GetCallMethod(ast, get_funcs_attr_status.ConsumeValueOrDie()));

  // Make sure "funcs" is accessible as a method.
  auto get_funcs_method_or_s = module->GetMethod("funcs");
  ASSERT_OK(get_funcs_method_or_s);
  auto funcs = get_funcs_method_or_s.ConsumeValueOrDie();
  auto funcs_result_or_s = funcs->Call({}, ast);
  ASSERT_OK(funcs_result_or_s);
  auto union_obj = funcs_result_or_s.ConsumeValueOrDie();
  EXPECT_MATCH(union_obj->node(), Union());

  // Make sure "free_var" is accessible as an attribute.
  auto get_free_var_attr_or_s = module->GetAttribute(ast, "free_var");
  ASSERT_OK(get_free_var_attr_or_s);
  auto free_var_obj = get_free_var_attr_or_s.ConsumeValueOrDie();
  EXPECT_MATCH(free_var_obj->node(), String("imfree"));
}

}  // namespace compiler
}  // namespace planner
}  // namespace carnot
}  // namespace px
