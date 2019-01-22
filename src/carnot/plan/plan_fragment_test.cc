#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>
#include <vector>

#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/proto/plan.pb.h"

namespace pl {
namespace carnot {
namespace plan {

using google::protobuf::TextFormat;

const char* kPlanFragmentWithFiveNodes = R"(
  id: 1,
  dag {
    nodes {
      id: 1
      sorted_deps: 2
      sorted_deps: 3
    }
    nodes {
      id: 2
      sorted_deps: 4
    }
    nodes {
      id: 3
      sorted_deps: 4
    }
    nodes {
      id: 4
      sorted_deps: 5
    }
    nodes {
      id: 5
    }
  }
  nodes {
    id: 1
    op {
      op_type: MEMORY_SOURCE_OPERATOR
      mem_source_op {
        name: "mem_source"
      }
    }
  }
  nodes {
    id: 2
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          constant {
            data_type: INT64
            int64_value: 1
          }
        }
        column_names: "test"
      }
    }
  }
  nodes {
    id: 3
    op {
      op_type: MAP_OPERATOR
      map_op {
        expressions {
          constant {
            data_type: INT64
            int64_value: 1
          }
        }
        column_names: "test2"
      }
    }
  }
  nodes {
    id: 4
    op {
      op_type: BLOCKING_AGGREGATE_OPERATOR
      blocking_agg_op {
        values {
          name: "blocking_agg"
        }
        value_names: "test3"
      }
    }
  }
  nodes {
    id: 5
    op {
      op_type: MEMORY_SINK_OPERATOR
      mem_sink_op {
        name: "mem_sink"
      }
    }
  }
)";

class PlanFragmentWalkerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    planpb::PlanFragment pf_pb;
    ASSERT_TRUE(TextFormat::MergeFromString(kPlanFragmentWithFiveNodes, &pf_pb));
    ASSERT_TRUE(plan_fragment_.Init(pf_pb).ok());
  }
  PlanFragment plan_fragment_ = PlanFragment(1);
};

TEST_F(PlanFragmentWalkerTest, basic_tests) {
  std::vector<int64_t> col_order;
  int mem_src_call_count = 0;
  int map_call_count = 0;
  int blocking_agg_call_count = 0;
  int mem_sink_call_count = 0;

  PlanFragmentWalker()
      .OnMemorySource([&](auto& mem_src) {
        col_order.push_back(mem_src.id());
        mem_src_call_count += 1;
      })
      .OnMap([&](auto& map) {
        col_order.push_back(map.id());
        map_call_count += 1;
      })
      .OnBlockingAggregate([&](auto& blocking_agg) {
        col_order.push_back(blocking_agg.id());
        blocking_agg_call_count += 1;
      })
      .OnMemorySink([&](auto& mem_sink) {
        col_order.push_back(mem_sink.id());
        mem_sink_call_count += 1;
      })
      .Walk(&plan_fragment_);
  EXPECT_EQ(1, mem_src_call_count);
  EXPECT_EQ(1, mem_sink_call_count);
  EXPECT_EQ(1, blocking_agg_call_count);
  EXPECT_EQ(2, map_call_count);
  EXPECT_EQ(std::vector<int64_t>({1, 2, 3, 4, 5}), col_order);
}

}  // namespace plan
}  // namespace carnot
}  // namespace pl
