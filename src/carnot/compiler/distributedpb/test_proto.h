#pragma once

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/substitute.h"
#include "src/carnot/compiler/distributedpb/distributed_plan.pb.h"
#include "src/carnot/plan/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/common/base/base.h"

namespace pl {
namespace carnot {
namespace compiler {
namespace distributedpb {
namespace testutils {

/**
 * This files provides canonical test protos that
 * other parts of the project can use to provide "fakes" for the
 * plan.
 *
 * Protos in this file are always valid as they are not expected to be used for
 * error case testing.
 */

const char* kSchema = R"proto(
relation_map {
  key: "table1"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
    }
    columns {
      column_name: "upid"
      column_type: UINT128
    }
  }
}

)proto";

const char* kAgentCarnotInfoTpl = R"proto(
query_broker_address: "$0"
has_grpc_server: false
has_data_store: true
processes_data: true
accepts_remote_sources: false
$1
)proto";

const char* kKelvinCarnotInfoTpl = R"proto(
query_broker_address: "$0"
grpc_address: "$1"
has_grpc_server: true
has_data_store: false
processes_data: true
accepts_remote_sources: true
)proto";

const char* kTableInfoTpl = R"proto(
table_info{
  table: "$0"
  tabletization_key: "$1"
  $2
}
)proto";

const char* kTabletValueTpl = R"proto(
tablets: "$0"
)proto";

const char* kQueryForTwoAgents = "dataframe(table = 'table1').result(name = 'out')";

distributedpb::DistributedState LoadDistributedStatePb(const std::string& distributed_state_str) {
  distributedpb::DistributedState distributed_state_pb;
  CHECK(
      google::protobuf::TextFormat::MergeFromString(distributed_state_str, &distributed_state_pb));
  return distributed_state_pb;
}

table_store::schemapb::Schema LoadSchemaPb(const std::string& schema_str) {
  table_store::schemapb::Schema schema_pb;
  CHECK(google::protobuf::TextFormat::MergeFromString(schema_str, &schema_pb));
  return schema_pb;
}

distributedpb::LogicalPlannerState LoadLogicalPlannerStatePB(
    const std::string& distributed_state_str, const std::string& schema_str) {
  distributedpb::LogicalPlannerState logical_planner_state_pb;
  *(logical_planner_state_pb.mutable_distributed_state()) =
      LoadDistributedStatePb(distributed_state_str);
  *(logical_planner_state_pb.mutable_schema()) = LoadSchemaPb(schema_str);
  return logical_planner_state_pb;
}

std::string MakeTableInfoStr(const std::string& table_name, const std::string& tabletization_key,
                             const std::vector<std::string>& tablets) {
  std::vector<std::string> formatted_tablets;
  for (const auto& t : tablets) {
    formatted_tablets.push_back(absl::Substitute(kTabletValueTpl, t));
  }
  return absl::Substitute(kTableInfoTpl, table_name, tabletization_key,
                          absl::StrJoin(formatted_tablets, "\n"));
}

std::string MakeAgentCarnotInfo(const std::string& agent_name,
                                const std::vector<std::string>& table_info) {
  return absl::Substitute(kAgentCarnotInfoTpl, agent_name, absl::StrJoin(table_info, "\n"));
}

std::string MakeKelvinCarnotInfo(const std::string& kelvin_name, const std::string& grpc_address) {
  return absl::Substitute(kKelvinCarnotInfoTpl, kelvin_name, grpc_address);
}

std::string MakeDistributedState(const std::vector<std::string>& carnot_info_strs) {
  std::vector<std::string> carnot_info_proto_strs;
  for (const auto& carnot_info : carnot_info_strs) {
    std::string proto_tpl = R"proto(carnot_info{
      $0
    })proto";
    carnot_info_proto_strs.push_back(absl::Substitute(proto_tpl, carnot_info));
  }
  return absl::StrJoin(carnot_info_proto_strs, "\n");
}

distributedpb::LogicalPlannerState CreateTwoAgentsPlannerState() {
  distributedpb::LogicalPlannerState plan;
  std::string table_name = "table1";
  std::string tabletization_key = "upid";
  std::string table_info1 = MakeTableInfoStr(table_name, tabletization_key, {"1", "2"});
  std::string table_info2 = MakeTableInfoStr(table_name, tabletization_key, {"3", "4"});
  std::string distributed_state_proto = MakeDistributedState(
      {MakeAgentCarnotInfo("agent1", {table_info1}), MakeAgentCarnotInfo("agent2", {table_info2})});

  return LoadLogicalPlannerStatePB(distributed_state_proto, kSchema);
}

distributedpb::LogicalPlannerState CreateOneAgentOneKelvinPlannerState() {
  distributedpb::LogicalPlannerState plan;
  std::string table_info1 = MakeTableInfoStr("table1", "upid", {"1", "2"});
  std::string distributed_state_proto = MakeDistributedState(
      {MakeAgentCarnotInfo("agent", {table_info1}), MakeKelvinCarnotInfo("agent", "1111")});

  return LoadLogicalPlannerStatePB(distributed_state_proto, kSchema);
}

distributedpb::LogicalPlannerState CreateTwoAgentsOneKelvinPlannerState() {
  distributedpb::LogicalPlannerState plan;
  std::string table_name = "table1";
  std::string tabletization_key = "upid";
  std::string table_info1 = MakeTableInfoStr(table_name, tabletization_key, {"1", "2"});
  std::string table_info2 = MakeTableInfoStr(table_name, tabletization_key, {"3", "4"});
  std::string distributed_state_proto = MakeDistributedState(
      {MakeAgentCarnotInfo("agent1", {table_info1}), MakeAgentCarnotInfo("agent2", {table_info2}),
       MakeKelvinCarnotInfo("kelvin", "1111")});

  return LoadLogicalPlannerStatePB(distributed_state_proto, kSchema);
}

const char* kExpectedPlanTwoAgents = R"proto(
qb_address_to_plan {
  key: "agent1"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 5
          sorted_children: 7
        }
        nodes {
          id: 3
          sorted_children: 7
        }
        nodes {
          id: 7
          sorted_children: 0
          sorted_parents: 3
          sorted_parents: 5
        }
        nodes {
          sorted_parents: 7
        }
      }
      nodes {
        id: 5
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "2"
          }
        }
      }
      nodes {
        id: 3
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "1"
          }
        }
      }
      nodes {
        id: 7
        op {
          op_type: UNION_OPERATOR
          union_op {
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
          }
        }
      }
      nodes {
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "out"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "agent2"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 5
          sorted_children: 7
        }
        nodes {
          id: 3
          sorted_children: 7
        }
        nodes {
          id: 7
          sorted_children: 0
          sorted_parents: 3
          sorted_parents: 5
        }
        nodes {
          sorted_parents: 7
        }
      }
      nodes {
        id: 5
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "4"
          }
        }
      }
      nodes {
        id: 3
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "3"
          }
        }
      }
      nodes {
        id: 7
        op {
          op_type: UNION_OPERATOR
          union_op {
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
          }
        }
      }
      nodes {
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "out"
          }
        }
      }
    }
  }
}
qb_address_to_dag_id {
  key: "agent1"
  value: 0
}
qb_address_to_dag_id {
  key: "agent2"
  value: 1
}
dag {
  nodes {
    id: 1
  }
  nodes {
  }
}
)proto";

const char* kExpectedPlanTwoAgentOneKelvin = R"proto(
qb_address_to_plan {
  key: "agent1"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 7
          sorted_children: 9
        }
        nodes {
          id: 5
          sorted_children: 9
        }
        nodes {
          id: 9
          sorted_children: 3
          sorted_parents: 5
          sorted_parents: 7
        }
        nodes {
          id: 3
          sorted_parents: 9
        }
      }
      nodes {
        id: 7
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "2"
          }
        }
      }
      nodes {
        id: 5
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "1"
          }
        }
      }
      nodes {
        id: 9
        op {
          op_type: UNION_OPERATOR
          union_op {
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
          }
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            address: "1111"
            destination_id: "agent1:0"
          }
        }
      }
    }
  }
}
qb_address_to_plan {
  key: "agent2"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 7
          sorted_children: 9
        }
        nodes {
          id: 5
          sorted_children: 9
        }
        nodes {
          id: 9
          sorted_children: 3
          sorted_parents: 5
          sorted_parents: 7
        }
        nodes {
          id: 3
          sorted_parents: 9
        }
      }
      nodes {
        id: 7
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "4"
          }
        }
      }
      nodes {
        id: 5
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "3"
          }
        }
      }
      nodes {
        id: 9
        op {
          op_type: UNION_OPERATOR
          union_op {
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
          }
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            address: "1111"
            destination_id: "agent2:0"
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
          id: 4
          sorted_children: 5
        }
        nodes {
          id: 3
          sorted_children: 5
        }
        nodes {
          id: 5
          sorted_children: 0
          sorted_parents: 3
          sorted_parents: 4
        }
        nodes {
          sorted_parents: 5
        }
      }
      nodes {
        id: 4
        op {
          op_type: GRPC_SOURCE_OPERATOR
          grpc_source_op {
            source_id: "agent1:0"
            column_types: TIME64NS
            column_types: INT64
            column_types: UINT128
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
          }
        }
      }
      nodes {
        id: 3
        op {
          op_type: GRPC_SOURCE_OPERATOR
          grpc_source_op {
            source_id: "agent2:0"
            column_types: TIME64NS
            column_types: INT64
            column_types: UINT128
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
          }
        }
      }
      nodes {
        id: 5
        op {
          op_type: UNION_OPERATOR
          union_op {
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
            column_mappings {
              column_indexes: 0
              column_indexes: 1
              column_indexes: 2
            }
          }
        }
      }
      nodes {
        op {
          op_type: MEMORY_SINK_OPERATOR
          mem_sink_op {
            name: "out"
            column_types: TIME64NS
            column_types: INT64
            column_types: UINT128
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
          }
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
  key: "kelvin"
  value: 0
}
dag {
  nodes {
    id: 2
    sorted_children: 0
  }
  nodes {
    id: 1
    sorted_children: 0
  }
  nodes {
    sorted_parents: 1
    sorted_parents: 2
  }
}
)proto";

}  // namespace testutils

}  // namespace distributedpb
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
