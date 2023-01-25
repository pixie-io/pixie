/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <memory>
#include <string>
#include <vector>

#include <absl/strings/str_replace.h>
#include <absl/strings/substitute.h>
#include "src/carnot/dag/dag.h"
#include "src/carnot/plan/plan_fragment.h"
#include "src/carnot/plan/plan_graph.h"
#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler/test_utils.h"
#include "src/carnot/planner/distributed/coordinator/coordinator.h"
#include "src/carnot/planner/distributed/distributed_plan/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributed/splitter/splitter.h"
#include "src/carnot/planner/distributedpb/distributed_plan.pb.h"
#include "src/carnot/udf_exporter/udf_exporter.h"
#include "src/common/base/base.h"
#include "src/common/uuid/uuid.h"
#include "src/shared/metadata/metadata_filter.h"

namespace px {
namespace carnot {
namespace planner {
namespace testutils {

using md::AgentMetadataFilter;
using ::px::testing::proto::EqualsProto;
using ::px::testing::proto::Partially;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::UnorderedElementsAre;
/**
 * This files provides canonical test protos that
 * other parts of the project can use to provide "fakes" for the
 * plan.
 *
 * Protos in this file are always valid as they are not expected to be used for
 * error case testing.
 */

constexpr char kSchema[] = R"proto(
relation_map {
  key: "table1"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
}

)proto";

constexpr char kHttpEventsSchema[] = R"proto(
relation_map {
  key: "http_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "major_version"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "minor_version"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "content_type"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_headers"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_method"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_path"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_headers"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_status"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_message"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_latency_ns"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "cpu"
  value {
    columns {
      column_name: "count"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu0"
      column_type: FLOAT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu1"
      column_type: FLOAT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu2"
      column_type: FLOAT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "process_stats"
  value {
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "cpu_ktime_ns"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_utime_ns"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "only_pem1"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
}
)proto";

constexpr char kConnStatsSchema[] = R"proto(
relation_map {
  key: "conn_stats"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "protocol"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_open"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_close"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_active"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "bytes_sent"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "bytes_recv"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "px_info_"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
)proto";

constexpr char kPEMCarnotInfoTpl[] = R"proto(
query_broker_address: "$0"
agent_id: {
  high_bits: $3
  low_bits: $4
}
has_grpc_server: false
has_data_store: true
processes_data: true
accepts_remote_sources: false
asid: $1
$2
)proto";

constexpr char kKelvinCarnotInfoTpl[] = R"proto(
query_broker_address: "$0"
agent_id: {
  high_bits: $3
  low_bits: $4
}
grpc_address: "$1"
has_grpc_server: true
has_data_store: false
processes_data: true
accepts_remote_sources: true
asid: $2
ssl_targetname: "kelvin.pl.svc"
)proto";

constexpr char kTableInfoTpl[] = R"proto(
table_info{
  table: "$0"
  tabletization_key: "$1"
  $2
}
)proto";

constexpr char kTabletValueTpl[] = R"proto(
tablets: "$0"
)proto";

constexpr char kHttpRequestStats[] = R"pxl(
import px

t1 = px.DataFrame(table='http_events', start_time='-30s')

t1['service'] = t1.ctx['service']
t1['http_resp_latency_ms'] = t1['resp_latency_ns'] / 1.0E6
t1['failure'] = t1['resp_status'] >= 400
t1['range_group'] = t1['time_'] - px.modulo(t1['time_'], 1000000000)

quantiles_agg = t1.groupby('service').agg(
  latency_quantiles=('http_resp_latency_ms', px.quantiles),
  errors=('failure', px.mean),
  throughput_total=('resp_status', px.count),
)

quantiles_agg['latency_p50'] = px.pluck(quantiles_agg['latency_quantiles'], 'p50')
quantiles_agg['latency_p90'] = px.pluck(quantiles_agg['latency_quantiles'], 'p90')
quantiles_agg['latency_p99'] = px.pluck(quantiles_agg['latency_quantiles'], 'p99')
quantiles_table = quantiles_agg[['service', 'latency_p50', 'latency_p90', 'latency_p99', 'errors', 'throughput_total']]

# The Range aggregate to calcualte the requests per second.
requests_agg = t1.groupby(['service', 'range_group']).agg(
  requests_per_window=('resp_status', px.count),
)

rps_table = requests_agg.groupby('service').agg(rps=('requests_per_window',px.mean))

joined_table = quantiles_table.merge(rps_table,
                                     how='inner',
                                     left_on=['service'],
                                     right_on=['service'],
                                     suffixes=['', '_x'])

joined_table['latency(p50)'] = joined_table['latency_p50']
joined_table['latency(p90)'] = joined_table['latency_p90']
joined_table['latency(p99)'] = joined_table['latency_p99']
joined_table['throughput (rps)'] = joined_table['rps']
joined_table['throughput total'] = joined_table['throughput_total']

joined_table = joined_table[[
  'service',
  'latency(p50)',
  'latency(p90)',
  'latency(p99)',
  'errors',
  'throughput (rps)',
  'throughput total']]
df = joined_table[joined_table['service'] != '']
px.display(df)
)pxl";

distributedpb::DistributedState LoadDistributedStatePb(const std::string& distributed_state_str) {
  distributedpb::DistributedState distributed_state_pb;
  CHECK(
      google::protobuf::TextFormat::MergeFromString(distributed_state_str, &distributed_state_pb));
  return distributed_state_pb;
}

table_store::schemapb::Schema LoadSchemaPb(std::string_view schema_str) {
  table_store::schemapb::Schema schema_pb;
  CHECK(google::protobuf::TextFormat::MergeFromString(schema_str.data(), &schema_pb));
  return schema_pb;
}

distributedpb::LogicalPlannerState LoadLogicalPlannerStatePB(
    const std::string& distributed_state_str, table_store::schemapb::Schema schema,
    const std::string& result_addr = "query-broker-ip:50300",
    const std::string& result_ssl_targetname = "query-broker-hostname") {
  distributedpb::LogicalPlannerState logical_planner_state_pb;
  auto distributed_info = logical_planner_state_pb.mutable_distributed_state();
  *distributed_info = LoadDistributedStatePb(distributed_state_str);
  std::vector<uuidpb::UUID> agent_list;
  for (int64_t i = 0; i < distributed_info->carnot_info_size(); ++i) {
    agent_list.push_back(distributed_info->carnot_info(i).agent_id());
  }

  for (const auto& [name, relation] : schema.relation_map()) {
    auto* schema_info = distributed_info->add_schema_info();
    schema_info->set_name(name);
    *(schema_info->mutable_relation()) = relation;
    for (const auto& agent_id : agent_list) {
      (*schema_info->add_agent_list()) = agent_id;
    }
  }
  logical_planner_state_pb.set_result_address(result_addr);
  logical_planner_state_pb.set_result_ssl_targetname(result_ssl_targetname);
  return logical_planner_state_pb;
}

distributedpb::LogicalPlannerState LoadLogicalPlannerStatePB(
    const std::string& distributed_state_str, std::string_view schema_str) {
  return LoadLogicalPlannerStatePB(distributed_state_str, LoadSchemaPb(schema_str));
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

std::string MakePEMCarnotInfo(const std::string& agent_name, const std::string& agent_id,
                              uint32_t asid, const std::vector<std::string>& table_info) {
  sole::uuid uuid = sole::rebuild(agent_id);
  return absl::Substitute(kPEMCarnotInfoTpl, agent_name, asid, absl::StrJoin(table_info, "\n"),
                          uuid.ab, uuid.cd);
}

std::string MakeKelvinCarnotInfo(const std::string& kelvin_name, const std::string& agent_id,
                                 const std::string& grpc_address, uint32_t asid) {
  sole::uuid uuid = sole::rebuild(agent_id);
  return absl::Substitute(kKelvinCarnotInfoTpl, kelvin_name, grpc_address, asid, uuid.ab, uuid.cd);
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

distributedpb::LogicalPlannerState CreateTwoPEMsPlannerState(table_store::schemapb::Schema schema) {
  distributedpb::LogicalPlannerState plan;
  std::string table_name = "table1";
  std::string tabletization_key = "upid";
  std::string table_info1 = MakeTableInfoStr(table_name, tabletization_key, {"1", "2"});
  std::string table_info2 = MakeTableInfoStr(table_name, tabletization_key, {"3", "4"});
  std::string distributed_state_proto = MakeDistributedState(
      {MakePEMCarnotInfo("pem1", "00000001-0000-0000-0000-000000000001", 123, {table_info1}),
       MakePEMCarnotInfo("pem2", "00000001-0000-0000-0000-000000000002", 456, {table_info2})});

  return LoadLogicalPlannerStatePB(distributed_state_proto, schema);
}

distributedpb::LogicalPlannerState CreateTwoPEMsPlannerState(std::string_view schema) {
  return CreateTwoPEMsPlannerState(LoadSchemaPb(schema));
}

distributedpb::LogicalPlannerState CreateTwoPEMsPlannerState() {
  return CreateTwoPEMsPlannerState(kSchema);
}

distributedpb::LogicalPlannerState CreateOnePEMOneKelvinPlannerState(
    table_store::schemapb::Schema schema) {
  distributedpb::LogicalPlannerState plan;
  std::string table_info1 = MakeTableInfoStr("table1", "upid", {"1", "2"});
  std::string distributed_state_proto = MakeDistributedState(
      {MakePEMCarnotInfo("pem", "00000001-0000-0000-0000-000000000001", 123, {table_info1}),
       MakeKelvinCarnotInfo("kelvin", "00000001-0000-0000-0000-000000000002", "1111", 456)});

  return LoadLogicalPlannerStatePB(distributed_state_proto, schema);
}

distributedpb::LogicalPlannerState CreateOnePEMOneKelvinPlannerState(std::string_view schema) {
  return CreateOnePEMOneKelvinPlannerState(LoadSchemaPb(schema));
}

distributedpb::LogicalPlannerState CreateOnePEMOneKelvinPlannerState() {
  return CreateOnePEMOneKelvinPlannerState(kSchema);
}

std::string TwoPEMsOneKelvinDistributedState() {
  std::string table_name = "table1";
  std::string tabletization_key = "upid";
  std::string table_info1 = MakeTableInfoStr(table_name, tabletization_key, {"1", "2"});
  std::string table_info2 = MakeTableInfoStr(table_name, tabletization_key, {"3", "4"});
  return MakeDistributedState(
      {MakePEMCarnotInfo("pem1", "00000001-0000-0000-0000-000000000001", 123, {table_info1}),
       MakePEMCarnotInfo("pem2", "00000001-0000-0000-0000-000000000002", 456, {table_info2}),
       MakeKelvinCarnotInfo("kelvin", "00000001-0000-0000-0000-000000000003", "1111", 789)});
}

std::string FourPEMsOneKelvinDistributedState() {
  std::string table_name = "table1";
  std::string table_info = "";
  return MakeDistributedState(
      {MakePEMCarnotInfo("pem1", "00000001-0000-0000-0000-000000000001", 123, {table_info}),
       MakePEMCarnotInfo("pem2", "00000001-0000-0000-0000-000000000002", 456, {table_info}),
       MakePEMCarnotInfo("pem3", "00000001-0000-0000-0000-000000000003", 000, {table_info}),
       MakePEMCarnotInfo("pem4", "00000001-0000-0000-0000-000000000004", 111, {table_info}),
       MakeKelvinCarnotInfo("kelvin", "00000001-0000-0000-0000-000000000003", "1111", 789)});
}

distributedpb::LogicalPlannerState CreateTwoPEMsOneKelvinPlannerState(const std::string& schema) {
  std::string distributed_state_proto = TwoPEMsOneKelvinDistributedState();
  return LoadLogicalPlannerStatePB(distributed_state_proto, schema);
}

distributedpb::LogicalPlannerState CreateTwoPEMsOneKelvinPlannerState(
    table_store::schemapb::Schema schema) {
  auto distributed_state_proto = TwoPEMsOneKelvinDistributedState();
  auto logical_state = LoadLogicalPlannerStatePB(distributed_state_proto, schema);
  return logical_state;
}

distributedpb::LogicalPlannerState CreateFourPEMsOneKelvinPlannerState(
    table_store::schemapb::Schema schema) {
  auto distributed_state_proto = FourPEMsOneKelvinDistributedState();
  return LoadLogicalPlannerStatePB(distributed_state_proto, schema);
}

distributedpb::LogicalPlannerState CreateTwoPEMsOneKelvinPlannerState() {
  return CreateTwoPEMsOneKelvinPlannerState(kSchema);
}

constexpr char kExpectedPlanTwoPEMs[] = R"proto(
qb_address_to_plan {
  key: "pem1"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 10
          sorted_children: 11
        }
        nodes {
          id: 9
          sorted_children: 11
        }
        nodes {
          id: 11
          sorted_children: 7
          sorted_parents: 9
          sorted_parents: 10
        }
        nodes {
          id: 7
          sorted_parents: 11
        }
      }
      nodes {
        id: 10
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "2"
          }
        }
      }
      nodes {
        id: 9
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "1"
          }
        }
      }
      nodes {
        id: 11
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
        id: 7
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
  key: "pem2"
  value {
    nodes {
      id: 1
      dag {
        nodes {
          id: 10
          sorted_children: 11
        }
        nodes {
          id: 9
          sorted_children: 11
        }
        nodes {
          id: 11
          sorted_children: 7
          sorted_parents: 9
          sorted_parents: 10
        }
        nodes {
          id: 7
          sorted_parents: 11
        }
      }
      nodes {
        id: 10
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "4"
          }
        }
      }
      nodes {
        id: 9
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            tablet: "3"
          }
        }
      }
      nodes {
        id: 11
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
        id: 7
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
  key: "pem1"
  value: 0
}
qb_address_to_dag_id {
  key: "pem2"
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

constexpr char kExpectedPlanOnePEMOneKelvin[] = R"proto(
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
          id: 17
          sorted_children: 14
        }
        nodes {
          id: 14
          sorted_parents: 17
        }
      }
      nodes {
        id: 17
        op {
          op_type: GRPC_SOURCE_OPERATOR
          grpc_source_op {
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
        id: 14
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            address: "query-broker-ip:50300"
            output_table {
              table_name: "out"
              column_types: TIME64NS
              column_types: INT64
              column_types: UINT128
              column_names: "time_"
              column_names: "cpu_cycles"
              column_names: "upid"
              column_semantic_types: ST_NONE
              column_semantic_types: ST_NONE
              column_semantic_types: ST_NONE
            }
            connection_options {
              ssl_targetname: "query-broker-hostname"
            }
          }
        }
      }
    }
    plan_options {
    }
    incoming_agent_ids {
      high_bits: 4294967296
      low_bits: 1
    }
  }
}
qb_address_to_plan {
  key: "pem"
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
          id: 12
          sorted_children: 15
        }
        nodes {
          id: 15
          sorted_parents: 12
        }
      }
      nodes {
        id: 12
        op {
          op_type: MEMORY_SOURCE_OPERATOR
          mem_source_op {
            name: "table1"
            column_idxs: 0
            column_idxs: 1
            column_idxs: 2
            column_names: "time_"
            column_names: "cpu_cycles"
            column_names: "upid"
            column_types: TIME64NS
            column_types: INT64
            column_types: UINT128
          }
        }
      }
      nodes {
        id: 15
        op {
          op_type: GRPC_SINK_OPERATOR
          grpc_sink_op {
            address: "1111"
            grpc_source_id: 17
            connection_options {
              ssl_targetname: "kelvin.pl.svc"
            }
          }
        }
      }
    }
    plan_options {
    }
  }
}
qb_address_to_dag_id {
  key: "kelvin"
  value: 0
}
qb_address_to_dag_id {
  key: "pem"
  value: 1
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

constexpr char kThreePEMsOneKelvinDistributedState[] = R"proto(
carnot_info {
  query_broker_address: "pem1"
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 123
  table_info {
    table: "table"
  }
}
carnot_info {
  query_broker_address: "pem2"
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 789
  table_info {
    table: "table"
  }
}
carnot_info {
  query_broker_address: "pem3"
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000003
  }
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 111
  table_info {
    table: "table"
  }
}
carnot_info {
  query_broker_address: "kelvin"
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000004
  }
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 456
  ssl_targetname: "kelvin.pl.svc"
}
schema_info {
  name: "table"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000003
  }
}
schema_info {
  name: "http_events"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000003
  }
}
schema_info {
  name: "process_stats"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000003
  }
}
schema_info {
  name: "only_pem1"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
}
)proto";

constexpr char kOnePEMOneKelvinDistributedState[] = R"proto(
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  query_broker_address: "pem"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 123
}
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  query_broker_address: "kelvin"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 456
  ssl_targetname: "kelvin.pl.svc"
}
schema_info {
  name: "table"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
}
)proto";

constexpr char kOnePEMThreeKelvinsDistributedState[] = R"proto(
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
  query_broker_address: "pem"
  has_grpc_server: false
  has_data_store: true
  processes_data: true
  accepts_remote_sources: false
  asid: 123
}
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000002
  }
  query_broker_address: "kelvin1"
  grpc_address: "1111"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 456
  ssl_targetname: "kelvin.pl.svc"
}
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000003
  }
  query_broker_address: "kelvin2"
  grpc_address: "1112"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 222
  ssl_targetname: "kelvin.pl.svc"
}
carnot_info {
  agent_id {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000004
  }
  query_broker_address: "kelvin3"
  grpc_address: "1113"
  has_grpc_server: true
  has_data_store: false
  processes_data: true
  accepts_remote_sources: true
  asid: 333
  ssl_targetname: "kelvin.pl.svc"
}
schema_info {
  name: "table"
  relation {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_cycles"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_NONE
    }
  }
  agent_list {
    high_bits: 0x0000000100000000
    low_bits: 0x0000000000000001
  }
}
)proto";

constexpr char kAllAgentsUDF[] = R"proto(
  name: "all_agents"
  return_type: STRING
  executor: UDF_ALL
)proto";

constexpr char kKelvinOnlyUDF[] = R"proto(
  name: "kelvin_only"
  return_type: STRING
  executor: UDF_KELVIN
)proto";

constexpr char kPEMOnlyUDF[] = R"proto(
  name: "pem_only"
  return_type: STRING
  executor: UDF_PEM
)proto";

constexpr char kKelvinOnlyWithArgUDF[] = R"proto(
  name: "kelvin_only"
  exec_arg_types: STRING
  return_type: STRING
  executor: UDF_KELVIN
)proto";

constexpr char kPEMOnlyWithArgUDF[] = R"proto(
  name: "pem_only"
  exec_arg_types: STRING
  return_type: STRING
  executor: UDF_PEM
)proto";

constexpr char kAllSchemas[] = R"proto(
relation_map {
  key: "conn_stats"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "addr_family"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "protocol"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "ssl"
      column_type: BOOLEAN
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_open"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_close"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "conn_active"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "bytes_sent"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "bytes_recv"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
  }
}
relation_map {
  key: "cql_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_op"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_op"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "dns_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_header"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_header"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "http_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "major_version"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "minor_version"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "content_type"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_headers"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_method"
      column_type: STRING
      column_semantic_type: ST_HTTP_REQ_METHOD
    }
    columns {
      column_name: "req_path"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body_size"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "resp_headers"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_status"
      column_type: INT64
      column_semantic_type: ST_HTTP_RESP_STATUS
    }
    columns {
      column_name: "resp_message"
      column_type: STRING
      column_semantic_type: ST_HTTP_RESP_MESSAGE
    }
    columns {
      column_name: "resp_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_body_size"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}

relation_map {
  key: "jvm_stats"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "young_gc_time"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
    columns {
      column_name: "full_gc_time"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
    columns {
      column_name: "used_heap_size"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "total_heap_size"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "max_heap_size"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "kafka_events.beta"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_cmd"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "client_id"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "mux_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_type"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "mysql_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_cmd"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_status"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp_body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "nats_events.beta"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cmd"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "body"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "network_stats"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "pod_id"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "rx_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "rx_packets"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "rx_errors"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "rx_drops"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "tx_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "tx_packets"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "tx_errors"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "tx_drops"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "pgsql_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_cmd"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "probe_status"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "source_connector"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "tracepoint"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "status"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "error"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "info"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "proc_exit_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "exit_code"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "signal"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "comm"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "process_stats"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "major_faults"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "minor_faults"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "cpu_utime_ns"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
    columns {
      column_name: "cpu_ktime_ns"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
    columns {
      column_name: "num_threads"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "vsize_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "rss_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "rchar_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "wchar_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "read_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
    columns {
      column_name: "write_bytes"
      column_type: INT64
      column_semantic_type: ST_BYTES
    }
  }
}
relation_map {
  key: "redis_events"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "remote_addr"
      column_type: STRING
      column_semantic_type: ST_IP_ADDRESS
    }
    columns {
      column_name: "remote_port"
      column_type: INT64
      column_semantic_type: ST_PORT
    }
    columns {
      column_name: "trace_role"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_cmd"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "req_args"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "resp"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "latency"
      column_type: INT64
      column_semantic_type: ST_DURATION_NS
    }
  }
}
relation_map {
  key: "stack_traces.beta"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "stack_trace_id"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "stack_trace"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "count"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
  }
}
relation_map {
  key: "stirling_error"
  value {
    columns {
      column_name: "time_"
      column_type: TIME64NS
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "upid"
      column_type: UINT128
      column_semantic_type: ST_UPID
    }
    columns {
      column_name: "source_connector"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "status"
      column_type: INT64
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "error"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
    columns {
      column_name: "context"
      column_type: STRING
      column_semantic_type: ST_NONE
    }
  }
}
)proto";

udfspb::UDFInfo UDFInfoWithTestUDTF() {
  auto udf_info = udfexporter::ExportUDFInfo().ConsumeValueOrDie()->info_pb();

  // Add source-specific test UDFs.
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("scalar_udfs{$0}", kKelvinOnlyUDF), &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("scalar_udfs{$0}", kPEMOnlyUDF), &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("scalar_udfs{$0}", kKelvinOnlyWithArgUDF), &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("scalar_udfs{$0}", kPEMOnlyWithArgUDF), &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("scalar_udfs{$0}", kAllAgentsUDF), &udf_info));

  // Add source-specific test UDTFs.
  CHECK(google::protobuf::TextFormat::MergeFromString(absl::Substitute("udtfs{$0}", kUDTFAllAgents),
                                                      &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("udtfs{$0}", kUDTFServiceUpTimePb), &udf_info));
  CHECK(google::protobuf::TextFormat::MergeFromString(
      absl::Substitute("udtfs{$0}", kUDTFOpenNetworkConnections), &udf_info));

  return udf_info;
}

std::unique_ptr<RelationMap> MakeRelationMap(const px::table_store::schemapb::Schema& schema_pb) {
  auto rel_map = std::make_unique<px::carnot::planner::RelationMap>();
  for (auto& relation_pair : schema_pb.relation_map()) {
    px::table_store::schema::Relation rel;
    PX_CHECK_OK(rel.FromProto(&relation_pair.second));
    rel_map->emplace(relation_pair.first, rel);
  }

  return rel_map;
}

class DistributedRulesTest : public OperatorTests {
 protected:
  void SetUpImpl() override {
    registry_info_ = std::make_unique<RegistryInfo>();
    logical_state_ = CreateTwoPEMsOneKelvinPlannerState(kHttpEventsSchema);

    ASSERT_OK(registry_info_->Init(UDFInfoWithTestUDTF()));
    compiler_state_ = std::make_unique<planner::CompilerState>(
        MakeRelationMap(LoadSchemaPb(kHttpEventsSchema)), planner::SensitiveColumnMap{},
        registry_info_.get(), /* time_now */ 1234,

        /* max_output_rows_per_table */ 0, "result_addr", "result_ssl_targetname",
        planner::RedactionOptions{}, nullptr, nullptr, planner::DebugInfo{});

    for (const auto& [plan_id, carnot_info] :
         Enumerate(logical_state_.distributed_state().carnot_info())) {
      sole::uuid uuid = ParseUUID(carnot_info.agent_id()).ConsumeValueOrDie();
      uuid_to_id_map_[uuid] = plan_id;
    }
  }

  std::shared_ptr<IR> CompileSingleNodePlan(std::string_view query) {
    compiler::Compiler compiler;
    return compiler.CompileToIR(std::string(query), compiler_state_.get()).ConsumeValueOrDie();
  }

  std::unique_ptr<distributed::DistributedPlan> CoordinateQuery(
      const std::string& query, const distributedpb::DistributedState& distributed_state) {
    // Create a CompilerState obj using the relation map and grabbing the current time.

    std::unique_ptr<distributed::Coordinator> coordinator =
        distributed::Coordinator::Create(compiler_state_.get(), distributed_state)
            .ConsumeValueOrDie();

    std::shared_ptr<IR> single_node_plan = CompileSingleNodePlan(query);

    std::unique_ptr<distributed::DistributedPlan> distributed_plan =
        coordinator->Coordinate(single_node_plan.get()).ConsumeValueOrDie();
    return distributed_plan;
  }

  std::unique_ptr<distributed::DistributedPlan> CoordinateQuery(const std::string& query) {
    return CoordinateQuery(query, logical_state_.distributed_state());
  }

  std::unique_ptr<distributed::DistributedPlan> PlanQuery(const std::string& query) {
    compiler::Compiler compiler;
    std::shared_ptr<IR> single_node_plan =
        compiler.CompileToIR(query, compiler_state_.get()).ConsumeValueOrDie();

    auto distributed_planner = distributed::DistributedPlanner::Create().ConsumeValueOrDie();
    return distributed_planner
        ->Plan(logical_state_.distributed_state(), compiler_state_.get(), single_node_plan.get())
        .ConsumeValueOrDie();
  }

  bool IsPEM(const distributedpb::CarnotInfo& carnot_instance) {
    return carnot_instance.has_data_store() && carnot_instance.processes_data() &&
           !carnot_instance.has_grpc_server();
  }

  distributedpb::DistributedState ThreeAgentOneKelvinStateWithMetadataInfo() {
    auto ps = LoadDistributedStatePb(kThreePEMsOneKelvinDistributedState);
    auto agent1_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();
    auto agent2_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();
    auto agent3_filter =
        AgentMetadataFilter::Create(100, 0.01, {MetadataType::POD_ID, MetadataType::SERVICE_ID})
            .ConsumeValueOrDie();

    PX_CHECK_OK(agent1_filter->InsertEntity(MetadataType::POD_ID, "agent1_pod"));
    PX_CHECK_OK(agent2_filter->InsertEntity(MetadataType::SERVICE_ID, "agent2_service"));

    absl::flat_hash_map<std::string, distributedpb::MetadataInfo> mds;
    mds["pem1"] = agent1_filter->ToProto();
    mds["pem2"] = agent2_filter->ToProto();
    mds["pem3"] = agent3_filter->ToProto();

    for (auto i = 0; i < ps.carnot_info_size(); ++i) {
      if (ps.carnot_info(i).query_broker_address() == "kelvin") {
        continue;
      }
      *(ps.mutable_carnot_info(i)->mutable_metadata_info()) =
          mds.at(ps.carnot_info(i).query_broker_address());
    }
    return ps;
  }

  std::unique_ptr<distributed::DistributedPlan> ThreeAgentOneKelvinCoordinateQuery(
      std::string_view query) {
    auto ps = ThreeAgentOneKelvinStateWithMetadataInfo();

    auto coordinator =
        distributed::Coordinator::Create(compiler_state_.get(), ps).ConsumeValueOrDie();
    compiler::Compiler compiler;
    auto graph =
        compiler.CompileToIR(std::string(query), compiler_state_.get()).ConsumeValueOrDie();

    return coordinator->Coordinate(graph.get()).ConsumeValueOrDie();
  }

  absl::flat_hash_set<int64_t> SourceNodeIds(distributed::DistributedPlan* plan) {
    absl::flat_hash_set<int64_t> node_ids;
    if (!plan) {
      return node_ids;
    }
    for (const auto& node : plan->dag().nodes()) {
      const auto& carnot_info = plan->Get(node)->carnot_info();
      if (!(carnot_info.has_data_store() && carnot_info.processes_data())) {
        continue;
      }
      node_ids.insert(node);
    }
    return node_ids;
  }

  /**
   * @brief Assembles the distributed plan from the distributed state.
   *
   * @param distributed_state
   * @return std::unique_ptr<DistributedPlan>
   */
  std::unique_ptr<distributed::DistributedPlan> AssembleDistributedPlan(
      const distributedpb::DistributedState& distributed_state) {
    auto distributed_plan = std::make_unique<distributed::DistributedPlan>();

    for (const auto& carnot_info : distributed_state.carnot_info()) {
      auto id = distributed_plan->AddCarnot(carnot_info).ConsumeValueOrDie();
      if (carnot_info.processes_data() && carnot_info.accepts_remote_sources()) {
        distributed_plan->SetKelvin(distributed_plan->Get(id));
      }
    }
    return distributed_plan;
  }

  /**
   * @brief Splits the plan using the distributed splitter.
   *
   * @param logical_plan
   * @return std::unique_ptr<BlockingSplitPlan>
   */
  std::unique_ptr<distributed::BlockingSplitPlan> SplitPlan(IR* logical_plan) {
    std::unique_ptr<distributed::Splitter> splitter =
        distributed::Splitter::Create(compiler_state_.get(),
                                      /* support_partial_agg */ false)
            .ConsumeValueOrDie();
    return splitter->SplitKelvinAndAgents(logical_plan).ConsumeValueOrDie();
  }

  std::unique_ptr<RegistryInfo> registry_info_;
  std::unique_ptr<CompilerState> compiler_state_;
  distributedpb::LogicalPlannerState logical_state_;
  absl::flat_hash_map<sole::uuid, int64_t> uuid_to_id_map_;
};

constexpr char kDependentRemovableOpsQuery[] = R"pxl(
import px
df = px.DataFrame(table='process_stats')
df = df[df.ctx['pod_id'] == "agent1_pod"]
px.display(df)
)pxl";

}  // namespace testutils
}  // namespace planner
}  // namespace carnot
}  // namespace px
