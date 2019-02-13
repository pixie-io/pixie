#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/exec_graph.h"
#include "src/carnot/plan/compiler_state.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/schema.h"
#include "src/carnot/proto/plan.pb.h"
#include "src/carnot/proto/test_proto.h"
#include "src/common/object_pool.h"

namespace pl {
namespace carnot {
namespace exec {

using google::protobuf::TextFormat;

class ExecGraphTest : public ::testing::Test {
 protected:
  void SetUp() override {
    carnotpb::PlanFragment pf_pb;
    ASSERT_TRUE(
        TextFormat::MergeFromString(carnotpb::testutils::kPlanFragmentWithFourNodes, &pf_pb));
    ASSERT_TRUE(plan_fragment_->Init(pf_pb).ok());
  }
  std::shared_ptr<plan::PlanFragment> plan_fragment_ = std::make_shared<plan::PlanFragment>(1);
};

TEST_F(ExecGraphTest, basic) {
  ExecutionGraph e;
  auto c = std::make_shared<plan::CompilerState>(std::make_shared<udf::ScalarUDFRegistry>("test"),
                                                 std::make_shared<udf::UDARegistry>("testUDA"));

  auto schema = std::make_shared<plan::Schema>();
  plan::Relation relation(std::vector<udf::UDFDataType>({udf::UDFDataType::INT64}),
                          std::vector<std::string>({"test"}));
  schema->AddRelation(1, relation);

  auto s = e.Init(schema, c, plan_fragment_);

  // Check that the structure of the exec graph is correct.
  auto sources = e.sources();
  EXPECT_EQ(1, sources.size());
  auto root = e.node(sources[0]).ConsumeValueOrDie();
  EXPECT_TRUE(root->IsSource());
  auto root_children = root->children();
  EXPECT_EQ(2, root_children.size());
  EXPECT_TRUE(root_children[0]->IsProcessing());
  EXPECT_TRUE(root_children[1]->IsProcessing());
  EXPECT_EQ(1, root_children[0]->children().size());
  EXPECT_EQ(1, root_children[1]->children().size());
  EXPECT_TRUE(root_children[0]->children()[0]->IsSink());
  EXPECT_EQ(root_children[1]->children()[0], root_children[0]->children()[0]);
  EXPECT_EQ(0, root_children[1]->children()[0]->children().size());
}

}  // namespace exec
}  // namespace carnot
}  // namespace pl
