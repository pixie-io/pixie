#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_plan.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/common/testing/protobuf.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using testing::proto::EqualsProto;
using testing::proto::Partially;

const char* kOneAgentDistributedState = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: false
  accepts_remote_sources: true
}
)proto";

class DistributedPlanTest : public OperatorTests {
 protected:
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }
};

const char* kIRProto = R"proto(
qb_address_to_plan {
  key: "agent"
  value {
    dag {
      nodes {
        id: 1
      }
    }
    nodes {
      id: 1
      dag {
        nodes {
          sorted_children: 2
        }
        nodes {
          id: 2
          sorted_parents: 0
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table"
            column_idxs: 0
            column_idxs: 1
            column_idxs: 2
            column_idxs: 3
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
          }
        }
      }
      nodes {
        id: 2
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "agent"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "kelvin"
  value {
    dag {
      nodes {
        id: 1
      }
    }
    nodes {
      id: 1
      dag {
        nodes {
          sorted_children: 2
        }
        nodes {
          id: 2
          sorted_parents: 0
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table"
            column_idxs: 0
            column_idxs: 1
            column_idxs: 2
            column_idxs: 3
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
          }
        }
      }
      nodes {
        id: 2
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "kelvin"
            column_types: INT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_types: FLOAT64
            column_names: "count"
            column_names: "cpu0"
            column_names: "cpu1"
            column_names: "cpu2"
          }
        }
      }
    }
  }
}
qb_address_to_dag_id {
  key: "agent"
  value: 0
}
qb_address_to_dag_id {
  key: "kelvin"
  value: 1
}
dag {
  nodes {
    sorted_children: 1
  }
  nodes {
    id: 1
    sorted_parents: 0
  }
}
)proto";

TEST_F(DistributedPlanTest, construction_test) {
  auto physical_plan = std::make_unique<DistributedPlan>();
  distributedpb::DistributedState physical_state =
      LoadDistributedStatePb(kOneAgentDistributedState);
  std::unordered_map<int64_t, distributedpb::CarnotInfo> carnot_id_to_carnot_info_map;
  for (int64_t i = 0; i < physical_state.carnot_info_size(); ++i) {
    distributedpb::CarnotInfo carnot_info = physical_state.carnot_info()[i];
    carnot_id_to_carnot_info_map.emplace(physical_plan->AddCarnot(carnot_info), carnot_info);
  }

  for (const auto& [carnot_id, carnot_info] : carnot_id_to_carnot_info_map) {
    CarnotInstance* carnot_instance = physical_plan->Get(carnot_id);
    EXPECT_THAT(carnot_instance->carnot_info(), EqualsProto(carnot_info.DebugString()));
    EXPECT_THAT(carnot_instance->QueryBrokerAddress(), carnot_info.query_broker_address());
    auto new_graph = std::make_shared<IR>();
    SwapGraphBeingBuilt(new_graph);
    auto mem_source = MakeMemSource(MakeRelation());
    auto mem_sink = MakeMemSink(mem_source, carnot_instance->QueryBrokerAddress());
    EXPECT_OK(mem_sink->SetRelation(MakeRelation()));
    carnot_instance->AddPlan(new_graph->Clone().ConsumeValueOrDie());

    auto carnot_plan_proto = carnot_instance->PlanProto().ConsumeValueOrDie();
  }

  physical_plan->AddEdge(physical_plan->Get(0), physical_plan->Get(1));

  auto physical_plan_proto = physical_plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(physical_plan_proto, EqualsProto(kIRProto));
}

}  // namespace distributed

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
