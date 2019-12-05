#include <gtest/gtest.h>
#include <memory>

#include "absl/container/flat_hash_map.h"

#include "src/carnot/compiler/objects/expr_object.h"
#include "src/carnot/compiler/objects/metadata_object.h"
#include "src/carnot/compiler/objects/pl_module.h"
#include "src/carnot/compiler/objects/test_utils.h"

namespace pl {
namespace carnot {
namespace compiler {
using ::pl::table_store::schema::Relation;
using ::testing::ElementsAre;

const char* kRegInfoProto = R"proto(
scalar_udfs {
  name: "pl.equals"
  exec_arg_types: UINT128
  exec_arg_types: UINT128
  return_type: BOOLEAN
}
)proto";
class DefaultArgumentsTest : public OperatorTests {
 protected:
  std::unique_ptr<compiler::RegistryInfo> SetUpRegistryInfo() {
    udfspb::UDFInfo udf_proto;
    google::protobuf::TextFormat::MergeFromString(kRegInfoProto, &udf_proto);

    auto info = std::make_unique<compiler::RegistryInfo>();
    PL_CHECK_OK(info->Init(udf_proto));
    return info;
  }

  std::unique_ptr<RelationMap> SetUpRelMap() {
    auto rel_map = std::make_unique<RelationMap>();
    rel_map->emplace("sequences", Relation(
                                      {
                                          types::TIME64NS,
                                          types::FLOAT64,
                                          types::FLOAT64,
                                      },
                                      {"time_", "xmod10", "PIx"}));
    return rel_map;
  }

  void SetUp() override {
    OperatorTests::SetUp();
    info_ = SetUpRegistryInfo();
    compiler_state_ = std::make_unique<CompilerState>(SetUpRelMap(), info_.get(), time_now_);
    module_ = PLModule::Create(graph.get(), compiler_state_.get()).ConsumeValueOrDie();
    ast_visitor_ = ASTVisitorImpl::Create(graph.get(), compiler_state_.get()).ConsumeValueOrDie();
  }

  void TestFunctionDefaults(std::shared_ptr<FuncObject> func_object, std::string_view function_str,
                            bool should_fail = false) {
    SCOPED_TRACE(function_str);
    for (const auto& [default_arg_name, default_arg_value] : func_object->defaults()) {
      auto expr_or_s = ast_visitor_->ParseAndProcessSingleExpression(default_arg_value);
      if (should_fail) {
        EXPECT_NOT_OK(expr_or_s);
      } else {
        EXPECT_OK(expr_or_s) << absl::Substitute(
            "for func '$0' the default value for argument '$1' failed to be parsed - '$2'",
            function_str, default_arg_name, default_arg_value);
      }
    }
  }

  void VerifyObjectDefaults(QLObjectPtr object, std::string_view name, bool should_fail = false) {
    SCOPED_TRACE(object->type_descriptor().name());
    for (const auto& [method_name, method_func] : object->methods()) {
      std::string path_str = absl::Substitute("$0->$1", name, method_name);
      TestFunctionDefaults(method_func, path_str, should_fail);
    }
    for (const auto& attr_name : object->AllAttributes()) {
      std::string path_str = absl::Substitute("$0->$1", name, attr_name);
      auto attr_object = object->GetAttribute(ast, attr_name).ConsumeValueOrDie();
      VerifyObjectDefaults(attr_object, path_str, should_fail);
    }
  }

  std::shared_ptr<ASTVisitorImpl> ast_visitor_;
  std::unique_ptr<CompilerState> compiler_state_;
  int64_t time_now_ = 1552607213931245000;
  std::unique_ptr<RegistryInfo> info_;
  std::shared_ptr<PLModule> module_;
};

// This test is the unit to make sure we can get all of the defaults of any method for any object
// we choose to test.
TEST_F(DefaultArgumentsTest, PLModule) {
  auto module_or_s = PLModule::Create(graph.get(), compiler_state_.get());
  ASSERT_OK(module_or_s);
  QLObjectPtr module = module_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("pl_module");
    VerifyObjectDefaults(module, "pl_module");
  }
}

TEST_F(DefaultArgumentsTest, Dataframe) {
  auto dataframe_or_s = Dataframe::Create(MakeMemSource());
  ASSERT_OK(dataframe_or_s);
  QLObjectPtr obj = dataframe_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Dataframe");
    VerifyObjectDefaults(obj, "Dataframe");
  }
}

TEST_F(DefaultArgumentsTest, Metadata) {
  auto metadata_or_s = MetadataObject::Create(MakeMemSource());
  ASSERT_OK(metadata_or_s);
  QLObjectPtr metadata = metadata_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Metadata");
    VerifyObjectDefaults(metadata, "Metadata");
  }
}

TEST_F(DefaultArgumentsTest, Expr) {
  auto expr_or_s = ExprObject::Create(MakeInt(10));
  ASSERT_OK(expr_or_s);
  QLObjectPtr expr = expr_or_s.ConsumeValueOrDie();
  {
    SCOPED_TRACE("Expr");
    VerifyObjectDefaults(expr, "Expr");
  }
}

/**
 * @brief This test has a syntax error inthe default argument of the only method. This makes sure
 * that we can fail upon finding this issue.
 *
 */
class TestQLObject : public QLObject {
 public:
  static constexpr TypeDescriptor TestQLObjectType = {
      /* name */ "TestQLObject",
      /* type */ QLObjectType::kMisc,
  };

  explicit TestQLObject(bool has_attr) : QLObject(TestQLObjectType) {
    std::shared_ptr<FuncObject> func_obj(new FuncObject(
        "default_func", {"arg"}, {{"arg", "d("}}, /*has_kwargs*/ false,
        std::bind(&TestQLObject::SimpleFunc, this, std::placeholders::_1, std::placeholders::_2)));
    AddMethod("default_func", func_obj);
    if (has_attr) {
      attributes_.emplace(kSpecialAttr);
    }
  }

  StatusOr<QLObjectPtr> SimpleFunc(const pypa::AstPtr&, const ParsedArgs&) {
    auto out_obj = std::make_shared<TestQLObject>(false);
    return StatusOr<QLObjectPtr>(out_obj);
  }

  StatusOr<QLObjectPtr> GetAttributeImpl(const pypa::AstPtr&,
                                         const std::string& name) const override {
    DCHECK(HasNonMethodAttribute(name));
    // Set to false so our test doesn't recurse forever.
    auto out_obj = std::make_shared<TestQLObject>(false);
    out_obj->SetName(name);
    return StatusOr<QLObjectPtr>(out_obj);
  }

  void SetName(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  inline static constexpr char kSpecialAttr[] = "foobar";

 private:
  std::string name_;
};

TEST_F(DefaultArgumentsTest, TestQLObject) {
  QLObjectPtr qlobject = std::make_shared<TestQLObject>(true);
  {
    SCOPED_TRACE("TestQLObject");
    VerifyObjectDefaults(qlobject, "TestQLObject", /* should_fail */ true);
  }
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
