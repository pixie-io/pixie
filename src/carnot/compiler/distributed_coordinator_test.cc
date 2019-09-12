#include <gmock/gmock.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include <pypa/parser/parser.hh>

#include "src/carnot/compiler/distributed_coordinator.h"
#include "src/carnot/compiler/distributed_planner.h"
#include "src/carnot/compiler/distributed_splitter.h"
#include "src/carnot/compiler/ir_nodes.h"
#include "src/carnot/compiler/metadata_handler.h"
#include "src/carnot/compiler/rule_mock.h"
#include "src/carnot/compiler/rules.h"
#include "src/carnot/compiler/test_utils.h"
#include "src/carnot/udf_exporter/udf_exporter.h"

#include "src/common/testing/protobuf.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributed {
using ::testing::ElementsAre;
using testing::proto::EqualsProto;
using testing::proto::Partially;

class CoordinatorTest : public OperatorTests {
 protected:
  distributedpb::DistributedState LoadDistributedStatePb(const std::string& physical_state_txt) {
    distributedpb::DistributedState physical_state_pb;
    CHECK(google::protobuf::TextFormat::MergeFromString(physical_state_txt, &physical_state_pb));
    return physical_state_pb;
  }

  void MakeGraph() {
    auto mem_src = MakeMemSource(MakeRelation());
    auto mem_sink = MakeMemSink(mem_src, "out");
    PL_CHECK_OK(mem_sink->SetRelation(MakeRelation()));
  }
};

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
  processes_data: true
  accepts_remote_sources: true
}
)proto";

const char* kOneAgentOneKelvinDistributedPlan = R"proto(
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
          id: 0
          sorted_children: 4
        }
        nodes {
          id: 4
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
        id: 4
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            destination_id: ":0"
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
          id: 3
          sorted_children: 2
        }
        nodes {
          id: 2
          sorted_parents: 3
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SOURCE_OPERATOR
          grpc_source_op {
            source_id: "0"
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
      nodes {
        id: 2
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "out"
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
  value: 1
}
qb_address_to_dag_id {
  key: "kelvin"
  value: 0
}
dag {
  nodes {
    id: 1
    sorted_children: 0
  }
  nodes {
    sorted_parents: 1
  }
}
)proto";

TEST_F(CoordinatorTest, one_agent_one_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto physical_plan_proto = physical_plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(physical_plan_proto, EqualsProto(kOneAgentOneKelvinDistributedPlan));
}

const char* kThreeAgentsOneKelvinDistributedState = R"proto(
carnot_info {
  query_broker_address: "agent1"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "agent2"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "agent3"
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
  processes_data: true
  accepts_remote_sources: true
}
)proto";

const char* kThreeAgentsOneKelvinDistributedPlan = R"proto(
  qb_address_to_plan {
  key: "agent1"
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
          sorted_children: 4
        }
        nodes {
          id: 4
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 4
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            destination_id: ":0"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "agent2"
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
          sorted_children: 4
        }
        nodes {
          id: 4
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 4
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            destination_id: ":0"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "agent3"
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
          sorted_children: 4
        }
        nodes {
          id: 4
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 4
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            destination_id: ":0"
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
          id: 3
          sorted_children: 2
        }
        nodes {
          id: 2
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 2
        op {
          op_type: MEMORY_SINK_OPERATOR
        }
      }
    }
  }
}
qb_address_to_dag_id {
  key: "agent1"
  value: 1
}
qb_address_to_dag_id {
  key: "agent2"
  value: 2
}
qb_address_to_dag_id {
  key: "agent3"
  value: 3
}
qb_address_to_dag_id {
  key: "kelvin"
  value: 0
}
dag {
  nodes {
    id: 3
    sorted_children: 0
  }
  nodes {
    id: 2
    sorted_children: 0
  }
  nodes {
    id: 1
    sorted_children: 0
  }
  nodes {
  }
}
)proto";

TEST_F(CoordinatorTest, three_agents_one_kelvin) {
  auto ps = LoadDistributedStatePb(kThreeAgentsOneKelvinDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();
  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  EXPECT_EQ(physical_plan->dag().nodes().size(), 4UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(3, 2, 1, 0));

  auto physical_plan_proto = physical_plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(physical_plan_proto, Partially(EqualsProto(kThreeAgentsOneKelvinDistributedPlan)));
}

const char* kOneAgentThreeKelvinsDistributedState = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
carnot_info {
  query_broker_address: "kelvin2"
  grpc_address: "1112"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
carnot_info {
  query_broker_address: "kelvin3"
  grpc_address: "1113"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
)proto";

const char* kOneAgentThreeKelvinsDistributedPlan = R"proto(
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
          sorted_children: 4
        }
        nodes {
          id: 4
        }
      }
      nodes {
        op {
          op_type: MEMORY_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 4
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            destination_id: ":0"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "kelvin1"
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
          id: 3
          sorted_children: 2
        }
        nodes {
          id: 2
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SOURCE_OPERATOR
        }
      }
      nodes {
        id: 2
        op {
          op_type: MEMORY_SINK_OPERATOR
        }
      }
    }
  }
}
qb_address_to_dag_id {
  key: "agent"
  value: 1
}
qb_address_to_dag_id {
  key: "kelvin1"
  value: 0
}
dag {
  nodes {
    id: 1
    sorted_children: 0
  }
  nodes {
  }
}
)proto";

TEST_F(CoordinatorTest, one_agent_three_kelvin) {
  auto ps = LoadDistributedStatePb(kOneAgentThreeKelvinsDistributedState);
  auto coordinator = Coordinator::Create(ps).ConsumeValueOrDie();

  MakeGraph();

  auto physical_plan = coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  ASSERT_EQ(physical_plan->dag().nodes().size(), 2UL);
  EXPECT_THAT(physical_plan->dag().TopologicalSort(), ElementsAre(1, 0));

  auto physical_plan_proto = physical_plan->ToProto().ConsumeValueOrDie();
  EXPECT_THAT(physical_plan_proto, Partially(EqualsProto(kOneAgentThreeKelvinsDistributedPlan)));
}

const char* kBadAgentSpecificationState = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: false
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
}
)proto";

TEST_F(CoordinatorTest, bad_agent_spec) {
  auto ps = LoadDistributedStatePb(kBadAgentSpecificationState);
  auto coordinator_status = Coordinator::Create(ps);

  ASSERT_NOT_OK(coordinator_status);
  EXPECT_EQ(coordinator_status.status().msg(),
            "Distributed state does not have a Carnot instance that satisifies the condition "
            "`has_data_store() && processes_data()`.");
}

const char* kBadKelvinSpecificationState = R"proto(
carnot_info {
  query_broker_address: "agent"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
}
carnot_info {
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: false
  accepts_remote_sources: true
}
)proto";

TEST_F(CoordinatorTest, bad_kelvin_spec) {
  auto ps = LoadDistributedStatePb(kBadKelvinSpecificationState);
  auto coordinator_status = Coordinator::Create(ps);

  ASSERT_NOT_OK(coordinator_status);
  EXPECT_EQ(coordinator_status.status().msg(),
            "Distributed state does not have a Carnot instance that satisifies the condition "
            "`processes_data() && accepts_remote_sources()`.");
}

}  // namespace distributed
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
