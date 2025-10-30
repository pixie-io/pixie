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

#include <clickhouse/client.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <parser.hpp>
#include <sole.hpp>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/exec_state.h"
#include "src/carnot/exec/local_grpc_result_server.h"
#include "src/carnot/funcs/funcs.h"
#include "src/common/base/base.h"
#include "src/common/testing/test_environment.h"
#include "src/common/testing/test_utils/container_runner.h"
#include "src/shared/types/column_wrapper.h"
#include "src/shared/types/type_utils.h"
#include "src/table_store/table_store.h"
#include "src/vizier/funcs/context/vizier_context.h"
#include "src/vizier/funcs/funcs.h"
#include "src/vizier/services/metadata/local/local_metadata_service.h"
#include "src/stirling/source_connectors/socket_tracer/http_table.h"

// Example clickhouse test usage:
// The records inserted into clickhouse exist between -10m and -5m
// bazel run -c dbg  src/carnot:carnot_executable --  --vmodule=clickhouse_source_node=1 --use_clickhouse=true     --query="import px;df = px.DataFrame('http_events', clickhouse=True, start_time='-10m', end_time='-9m'); px.display(df)"     --output_file=$(pwd)/output.csv
//
// Testing existing ClickHouse table (kubescape_stix) table population and query:
// bazel run -c dbg  src/carnot:carnot_executable --  --vmodule=clickhouse_source_node=1 --use_clickhouse=true --start_clickhouse=false --query="import px;df = px.DataFrame('kubescape_stix', clickhouse=True, start_time='-10m'); px.display(df)"     --output_file=$(pwd)/output.csv


DEFINE_string(input_file, gflags::StringFromEnv("INPUT_FILE", ""),
              "The csv containing data to run the query on.");

DEFINE_string(output_file, gflags::StringFromEnv("OUTPUT_FILE", ""),
              "The file path to write the output data to.");

DEFINE_string(query, gflags::StringFromEnv("QUERY", ""), "The query to run.");

DEFINE_string(table_name, gflags::StringFromEnv("TABLE_NAME", "csv_table"),
              "The name of the table to store the csv data.");

DEFINE_int64(rowbatch_size, gflags::Int64FromEnv("ROWBATCH_SIZE", 100),
             "The size of the rowbatches.");

DEFINE_bool(use_clickhouse, gflags::BoolFromEnv("USE_CLICKHOUSE", false),
            "Whether to populate a ClickHouse database.");

DEFINE_bool(start_clickhouse, gflags::BoolFromEnv("START_CLICKHOUSE", true),
            "Whether to start a ClickHouse container with test data.");

using px::types::DataType;

namespace {
/**
 * Gets the corresponding px::DataType from the string type in the csv.
 * @param type the string from the csv.
 * @return the px::DataType.
 */
px::StatusOr<DataType> GetTypeFromHeaderString(const std::string& type) {
  if (type == "int64") {
    return DataType::INT64;
  }
  if (type == "uint128") {
    return DataType::UINT128;
  }
  if (type == "float64") {
    return DataType::FLOAT64;
  }
  if (type == "boolean") {
    return DataType::BOOLEAN;
  }
  if (type == "string") {
    return DataType::STRING;
  }
  if (type == "time64ns") {
    return DataType::TIME64NS;
  }
  return px::error::InvalidArgument("Could not recognize type '$0' from header.", type);
}

std::string ValueToString(int64_t val) { return absl::Substitute("$0", val); }
std::string ValueToString(absl::uint128 val) {
  return absl::Substitute("$0:$1", absl::Uint128High64(val), absl::Uint128Low64(val));
}

std::string ValueToString(double val) { return absl::StrFormat("%.2f", val); }

std::string ValueToString(std::string val) { return val; }

std::string ValueToString(bool val) { return val ? "true" : "false"; }

/**
 * Takes the value and converts it to the string representation.
 * @ param type The type of the value.
 * @ param val The value.
 * @return The string representation.
 */
template <DataType DT>
void AddStringValueToRow(std::vector<std::string>* row, arrow::Array* arr, int64_t idx) {
  using ArrowArrayType = typename px::types::DataTypeTraits<DT>::arrow_array_type;

  auto val = ValueToString(px::types::GetValue(static_cast<ArrowArrayType*>(arr), idx));
  row->push_back(val);
}

/**
 * Convert the csv at the given filename into a Carnot table.
 * @param filename The filename of the csv to convert.
 * @return The Carnot table.
 */
std::shared_ptr<px::table_store::Table> GetTableFromCsv(const std::string& filename,
                                                        int64_t rb_size) {
  std::ifstream f(filename);
  aria::csv::CsvParser parser(f);

  // The schema of the columns.
  std::vector<px::types::DataType> types;
  // The names of the columns.
  std::vector<std::string> names;

  // Get the columns types and names.
  auto row_idx = 0;
  for (auto& row : parser) {
    for (auto& field : row) {
      if (row_idx == 0) {
        auto type = GetTypeFromHeaderString(field).ConsumeValueOrDie();
        // Currently reading the first row, which should be the types of the columns.
        types.push_back(type);
      } else if (row_idx == 1) {  // Reading second row, should be the names of columns.
        names.push_back(field);
      }
    }
    row_idx++;
    if (row_idx > 1) {
      break;
    }
  }

  // Construct the table.
  px::table_store::schema::Relation rel(types, names);
  auto table = px::table_store::Table::Create("csv_table", rel);

  // Add rowbatches to the table.
  row_idx = 0;
  std::unique_ptr<std::vector<px::types::SharedColumnWrapper>> batch;
  for (auto& row : parser) {
    if (row_idx % rb_size == 0) {
      if (batch) {
        auto s = table->TransferRecordBatch(std::move(batch));
        if (!s.ok()) {
          LOG(ERROR) << "Couldn't add record batch to table.";
        }
      }

      // Create new batch.
      batch = std::make_unique<std::vector<px::types::SharedColumnWrapper>>();
      // Create vectors for each column.
      for (auto type : types) {
        auto wrapper = px::types::ColumnWrapper::Make(type, 0);
        batch->push_back(wrapper);
      }
    }
    auto col_idx = 0;
    for (auto& field : row) {
      switch (types[col_idx]) {
        case DataType::INT64:
          static_cast<px::types::Int64ValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::stoi(field));
          break;
        case DataType::FLOAT64:
          static_cast<px::types::Float64ValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::stof(field));
          break;
        case DataType::BOOLEAN:
          static_cast<px::types::BoolValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(field == "true");
          break;
        case DataType::STRING:
          static_cast<px::types::StringValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::string(field));
          break;
        case DataType::TIME64NS:
          static_cast<px::types::Time64NSValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::stoi(field));
          break;
        default:
          LOG(ERROR) << "Couldn't convert field to a ValueType.";
      }
      col_idx++;
    }
    row_idx++;
  }
  // Add the final batch to the table.
  if (batch->at(0)->Size() > 0) {
    auto s = table->TransferRecordBatch(std::move(batch));
    if (!s.ok()) {
      LOG(ERROR) << "Couldn't add record batch to table.";
    }
  }

  return table;
}

/**
 * Write the table to a CSV.
 * @param filename The name of the output CSV file.
 * @param table The table to write to a CSV.
 */
void TableToCsv(const std::string& filename,
                const std::vector<px::carnot::RowBatch> result_batches) {
  std::ofstream output_csv;
  output_csv.open(filename);
  if (!result_batches.size()) {
    output_csv.close();
  }
  for (const auto& rb : result_batches) {
    for (auto row_idx = 0; row_idx < rb.num_rows(); row_idx++) {
      std::vector<std::string> row;
      for (auto col_idx = 0; col_idx < rb.num_columns(); col_idx++) {
#define TYPE_CASE(_dt_) AddStringValueToRow<_dt_>(&row, rb.ColumnAt(col_idx).get(), row_idx)
        PX_SWITCH_FOREACH_DATATYPE(rb.desc().type(col_idx), TYPE_CASE);
#undef TYPE_CASE
      }
      output_csv << absl::StrJoin(row, ",") << "\n";
    }
  }
  output_csv.close();
}

// ClickHouse container configuration
constexpr char kClickHouseImage[] =
    "src/stirling/source_connectors/socket_tracer/testing/container_images/clickhouse.tar";
constexpr char kClickHouseReadyMessage[] = "Ready for connections";
constexpr int kClickHousePort = 9000;

/**
 * Sets up a ClickHouse client connection with retries.
 */
std::unique_ptr<clickhouse::Client> SetupClickHouseClient() {
  clickhouse::ClientOptions client_options;
  client_options.SetHost("localhost");
  client_options.SetPort(kClickHousePort);
  client_options.SetUser("default");
  client_options.SetPassword("test_password");
  client_options.SetDefaultDatabase("default");

  const int kMaxRetries = 10;
  for (int i = 0; i < kMaxRetries; ++i) {
    LOG(INFO) << "Attempting to connect to ClickHouse (attempt " << (i + 1) << "/" << kMaxRetries
              << ")...";
    try {
      auto client = std::make_unique<clickhouse::Client>(client_options);
      client->Execute("SELECT 1");
      LOG(INFO) << "Successfully connected to ClickHouse";
      return client;
    } catch (const std::exception& e) {
      LOG(WARNING) << "Failed to connect: " << e.what();
      if (i < kMaxRetries - 1) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
      } else {
        LOG(FATAL) << "Failed to connect to ClickHouse after " << kMaxRetries << " attempts";
      }
    }
  }
  return nullptr;
}

/**
 * Creates the http_events table in ClickHouse with proper schema and sample data.
 */
void PopulateHttpEventsTable(clickhouse::Client* client) {
  try {
    // Get current hostname for the data
    char current_hostname[256];
    gethostname(current_hostname, sizeof(current_hostname));
    std::string hostname_str(current_hostname);

    // Insert sample data matching the stirling HTTP table schema (upid as String with high:low format)
    auto time_col = std::make_shared<clickhouse::ColumnDateTime64>(9);
    auto upid_col = std::make_shared<clickhouse::ColumnString>();
    auto remote_addr_col = std::make_shared<clickhouse::ColumnString>();
    auto remote_port_col = std::make_shared<clickhouse::ColumnInt64>();
    auto local_addr_col = std::make_shared<clickhouse::ColumnString>();
    auto local_port_col = std::make_shared<clickhouse::ColumnInt64>();
    auto trace_role_col = std::make_shared<clickhouse::ColumnInt64>();
    auto encrypted_col = std::make_shared<clickhouse::ColumnUInt8>();  // Boolean
    auto major_version_col = std::make_shared<clickhouse::ColumnInt64>();
    auto minor_version_col = std::make_shared<clickhouse::ColumnInt64>();
    auto content_type_col = std::make_shared<clickhouse::ColumnInt64>();
    auto req_headers_col = std::make_shared<clickhouse::ColumnString>();
    auto req_method_col = std::make_shared<clickhouse::ColumnString>();
    auto req_path_col = std::make_shared<clickhouse::ColumnString>();
    auto req_body_col = std::make_shared<clickhouse::ColumnString>();
    auto req_body_size_col = std::make_shared<clickhouse::ColumnInt64>();
    auto resp_headers_col = std::make_shared<clickhouse::ColumnString>();
    auto resp_status_col = std::make_shared<clickhouse::ColumnInt64>();
    auto resp_message_col = std::make_shared<clickhouse::ColumnString>();
    auto resp_body_col = std::make_shared<clickhouse::ColumnString>();
    auto resp_body_size_col = std::make_shared<clickhouse::ColumnInt64>();
    auto latency_col = std::make_shared<clickhouse::ColumnInt64>();
#ifndef NDEBUG
    auto px_info_col = std::make_shared<clickhouse::ColumnString>();
#endif
    auto hostname_col = std::make_shared<clickhouse::ColumnString>();
    auto event_time_col = std::make_shared<clickhouse::ColumnDateTime64>(3);

    // Add sample rows
    std::time_t now = std::time(nullptr);
    LOG(INFO) << "Current time: " << now;

    // Add 10 records (5 with current hostname, 5 with different hostnames)
    for (int i = 0; i < 10; ++i) {
      time_col->Append((now - 600 + i * 60) * 1000000000LL);  // Convert to nanoseconds

      // Generate upid as UINT128 in high:low string format
      uint64_t upid_high = 1000 + i;
      uint64_t upid_low = 2000 + i;
      upid_col->Append(absl::StrFormat("%d:%d", upid_high, upid_low));

      remote_addr_col->Append(absl::StrFormat("192.168.1.%d", 100 + i));
      remote_port_col->Append(50000 + i);
      local_addr_col->Append("127.0.0.1");
      local_port_col->Append(8080);

      // trace_role: 1 = server, 2 = client (alternate)
      trace_role_col->Append(i % 2 == 0 ? 1 : 2);

      // encrypted: false for most, true for some
      encrypted_col->Append(i % 3 == 0 ? 1 : 0);

      major_version_col->Append(1);
      minor_version_col->Append(1);
      content_type_col->Append(i % 2 == 0 ? 1 : 0);  // 1 = JSON, 0 = unknown

      req_headers_col->Append("Content-Type: application/json");
      req_method_col->Append(i % 2 == 0 ? "GET" : "POST");
      req_path_col->Append(absl::StrFormat("/api/v1/resource/%d", i));

      std::string req_body = i % 2 == 0 ? "" : "{\"data\": \"test\"}";
      req_body_col->Append(req_body);
      req_body_size_col->Append(req_body.size());

      resp_headers_col->Append("Content-Type: application/json");
      resp_status_col->Append(200);
      resp_message_col->Append("OK");

      std::string resp_body = "{\"result\": \"success\"}";
      resp_body_col->Append(resp_body);
      resp_body_size_col->Append(resp_body.size());

      latency_col->Append(1000000 + i * 100000);
#ifndef NDEBUG
      px_info_col->Append("");
#endif

      // First 5 use current hostname, next 5 use different hostnames
      if (i < 5) {
        hostname_col->Append(hostname_str);
      } else {
        hostname_col->Append(absl::StrFormat("other-host-%d", i % 3));
      }

      event_time_col->Append((now - 600 + i * 60) * 1000LL);  // Convert to milliseconds
    }

    clickhouse::Block block;
    block.AppendColumn("time_", time_col);
    block.AppendColumn("upid", upid_col);
    block.AppendColumn("remote_addr", remote_addr_col);
    block.AppendColumn("remote_port", remote_port_col);
    block.AppendColumn("local_addr", local_addr_col);
    block.AppendColumn("local_port", local_port_col);
    block.AppendColumn("trace_role", trace_role_col);
    block.AppendColumn("encrypted", encrypted_col);
    block.AppendColumn("major_version", major_version_col);
    block.AppendColumn("minor_version", minor_version_col);
    block.AppendColumn("content_type", content_type_col);
    block.AppendColumn("req_headers", req_headers_col);
    block.AppendColumn("req_method", req_method_col);
    block.AppendColumn("req_path", req_path_col);
    block.AppendColumn("req_body", req_body_col);
    block.AppendColumn("req_body_size", req_body_size_col);
    block.AppendColumn("resp_headers", resp_headers_col);
    block.AppendColumn("resp_status", resp_status_col);
    block.AppendColumn("resp_message", resp_message_col);
    block.AppendColumn("resp_body", resp_body_col);
    block.AppendColumn("resp_body_size", resp_body_size_col);
    block.AppendColumn("latency", latency_col);
    block.AppendColumn("hostname", hostname_col);
    block.AppendColumn("event_time", event_time_col);

    client->Insert("http_events", block);
    LOG(INFO) << "http_events table populated successfully with 10 records";
  } catch (const std::exception& e) {
    LOG(FATAL) << "Failed to populate http_events table: " << e.what();
  }
}

/**
 * Checks if a table exists in ClickHouse.
 */
bool TableExists(clickhouse::Client* client, const std::string& table_name) {
  try {
    std::string query = absl::Substitute("EXISTS TABLE $0", table_name);
    bool exists = false;
    client->Select(query, [&exists](const clickhouse::Block& block) {
      if (block.GetRowCount() > 0) {
        auto result_col = block[0]->As<clickhouse::ColumnUInt8>();
        exists = result_col->At(0) == 1;
      }
    });
    return exists;
  } catch (const std::exception& e) {
    LOG(WARNING) << "Failed to check if table " << table_name << " exists: " << e.what();
    return false;
  }
}

/**
 * Populates the kubescape_stix table with sample STIX data if it exists.
 */
void PopulateKubescapeStixTable(clickhouse::Client* client) {
  try {
    // Check if table exists
    if (!TableExists(client, "kubescape_stix")) {
      LOG(INFO) << "kubescape_stix table does not exist, skipping population";
      return;
    }

    LOG(INFO) << "Populating kubescape_stix table with sample data...";

    // Get current hostname
    char current_hostname[256];
    gethostname(current_hostname, sizeof(current_hostname));
    std::string hostname_str(current_hostname);

    // Create columns for the kubescape_stix table
    auto timestamp_col = std::make_shared<clickhouse::ColumnString>();
    auto pod_name_col = std::make_shared<clickhouse::ColumnString>();
    auto namespace_col = std::make_shared<clickhouse::ColumnString>();
    auto data_col = std::make_shared<clickhouse::ColumnString>();
    auto hostname_col = std::make_shared<clickhouse::ColumnString>();
    auto event_time_col = std::make_shared<clickhouse::ColumnDateTime64>(3);

    // Add sample STIX data
    std::time_t now = std::time(nullptr);

    // Add 5 sample records with different pods and namespaces
    std::vector<std::string> pod_names = {"web-pod-1", "api-pod-2", "db-pod-3", "cache-pod-4", "worker-pod-5"};
    std::vector<std::string> namespaces = {"production", "staging", "development", "production", "staging"};

    for (int i = 0; i < 5; ++i) {
      // Timestamp as ISO 8601 string
      std::time_t record_time = now - (300 - i * 60);  // 5 minutes ago to 1 minute ago
      char time_buf[30];
      std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&record_time));
      timestamp_col->Append(std::string(time_buf));

      pod_name_col->Append(pod_names[i]);
      namespace_col->Append(namespaces[i]);

      // Add unique STIX data for each record
      std::string stix_data = absl::Substitute(
          R"({"type":"bundle","id":"bundle--$0","objects":[{"type":"vulnerability","id":"vuln--$0","severity":"$1"}]})",
          i, (i % 3 == 0 ? "high" : "medium"));
      data_col->Append(stix_data);

      hostname_col->Append(hostname_str);
      event_time_col->Append(record_time * 1000LL);  // Convert to milliseconds
    }

    // Create block and insert
    clickhouse::Block block;
    block.AppendColumn("timestamp", timestamp_col);
    block.AppendColumn("pod_name", pod_name_col);
    block.AppendColumn("namespace", namespace_col);
    block.AppendColumn("data", data_col);
    block.AppendColumn("hostname", hostname_col);
    block.AppendColumn("event_time", event_time_col);

    client->Insert("kubescape_stix", block);
    LOG(INFO) << "kubescape_stix table populated successfully with 5 records";
  } catch (const std::exception& e) {
    LOG(WARNING) << "Failed to populate kubescape_stix table: " << e.what();
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  px::EnvironmentGuard env_guard(&argc, argv);

  auto filename = FLAGS_input_file;
  auto output_filename = FLAGS_output_file;
  auto query = FLAGS_query;
  auto rb_size = FLAGS_rowbatch_size;
  auto table_name = FLAGS_table_name;
  auto use_clickhouse = FLAGS_use_clickhouse;

  // ClickHouse container and client (if enabled)
  std::unique_ptr<px::ContainerRunner> clickhouse_server;
  std::unique_ptr<clickhouse::Client> clickhouse_client;

  std::shared_ptr<px::table_store::Table> table;

  if (use_clickhouse) {

    if (FLAGS_start_clickhouse) {
      LOG(INFO) << "Starting ClickHouse container...";
      clickhouse_server =
          std::make_unique<px::ContainerRunner>(px::testing::BazelRunfilePath(kClickHouseImage),
                                                "clickhouse_carnot", kClickHouseReadyMessage);

      std::vector<std::string> options = {
          absl::Substitute("--publish=$0:$0", kClickHousePort),
          "--env=CLICKHOUSE_PASSWORD=test_password",
          "--network=host",
      };

      auto status = clickhouse_server->Run(std::chrono::seconds{60}, options, {}, true,
                                           std::chrono::seconds{300});
      if (!status.ok()) {
        LOG(FATAL) << "Failed to start ClickHouse container: " << status.msg();
      }
    }

    // Give ClickHouse time to initialize
    LOG(INFO) << "Waiting for ClickHouse to initialize...";
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Setup ClickHouse client and create test table
    clickhouse_client = SetupClickHouseClient();
    LOG(INFO) << "ClickHouse ready with http_events table";
  } else {
    // Only load CSV if not using ClickHouse
    table = GetTableFromCsv(filename, rb_size);
  }

  // Execute query.
  auto table_store = std::make_shared<px::table_store::TableStore>();
  auto result_server = px::carnot::exec::LocalGRPCResultSinkServer();

  // Create metadata service stub for table schemas
  auto metadata_grpc_server = std::make_unique<px::vizier::services::metadata::LocalMetadataGRPCServer>(table_store.get());

  // Create vizier func factory context with metadata stub
  px::vizier::funcs::VizierFuncFactoryContext func_context(
      nullptr,  // agent_manager
      metadata_grpc_server->StubGenerator(),  // mds_stub
      nullptr,  // mdtp_stub
      nullptr,  // cronscript_stub
      table_store,
      [](grpc::ClientContext*) {}  // add_grpc_auth
  );

  auto func_registry = std::make_unique<px::carnot::udf::Registry>("default_registry");
  px::vizier::funcs::RegisterFuncsOrDie(func_context, func_registry.get());

  auto clients_config =
      std::make_unique<px::carnot::Carnot::ClientsConfig>(px::carnot::Carnot::ClientsConfig{
          [&result_server](const std::string& address, const std::string&) {
            return result_server.StubGenerator(address);
          },
          [](grpc::ClientContext*) {},
      });
  auto server_config = std::make_unique<px::carnot::Carnot::ServerConfig>();
  server_config->grpc_server_creds = grpc::InsecureServerCredentials();
  server_config->grpc_server_port = 0;

  auto carnot = px::carnot::Carnot::Create(sole::uuid4(), std::move(func_registry), table_store,
                                           std::move(clients_config), std::move(server_config))
                    .ConsumeValueOrDie();

  if (use_clickhouse) {
    // Create http_events table schema in table_store using the actual stirling HTTP table definition
    std::vector<px::types::DataType> types;
    std::vector<std::string> names;

    // Convert stirling DataTableSchema to table_store Relation
    for (const auto& element : px::stirling::kHTTPTable.elements()) {
      std::string col_name(element.name());
      types.push_back(element.type());
      names.push_back(col_name);
    }

    px::table_store::schema::Relation rel(types, names);
    auto http_events_table = px::table_store::Table::Create("http_events", rel);
    // Need to provide a table_id for GetTableIDs() to work
    uint64_t http_events_table_id = 1;
    table_store->AddTable(http_events_table, "http_events", http_events_table_id);

    // Log the schema for debugging
    LOG(INFO) << "http_events table schema has " << names.size() << " columns:";
    for (size_t i = 0; i < names.size(); ++i) {
      LOG(INFO) << "  Column[" << i << "]: " << names[i] << " (type=" << static_cast<int>(types[i]) << ")";
    }

    auto schema_query = "import px; px.display(px.CreateClickHouseSchemas())";
    auto schema_query_status = carnot->ExecuteQuery(schema_query, sole::uuid4(), px::CurrentTimeNS());
    if (!schema_query_status.ok()) {
      LOG(FATAL) << absl::Substitute("Schema query failed to execute: $0",
                                     schema_query_status.msg());
    }
    PopulateHttpEventsTable(clickhouse_client.get());
    PopulateKubescapeStixTable(clickhouse_client.get());
  } else if (table != nullptr) {
    // Add CSV table to table_store
    table_store->AddTable(table_name, table);
  }

  auto exec_status = carnot->ExecuteQuery(query, sole::uuid4(), px::CurrentTimeNS());
  if (!exec_status.ok()) {
    LOG(FATAL) << absl::Substitute("Query failed to execute: $0", exec_status.msg());
  }

  // Get and log execution stats
  auto exec_stats_or = result_server.exec_stats();
  if (exec_stats_or.ok()) {
    auto exec_stats = exec_stats_or.ConsumeValueOrDie();
    if (exec_stats.has_execution_stats()) {
      auto stats = exec_stats.execution_stats();
      LOG(INFO) << "Query Execution Stats:";
      LOG(INFO) << "  Bytes processed: " << stats.bytes_processed();
      LOG(INFO) << "  Records processed: " << stats.records_processed();
      if (stats.has_timing()) {
        LOG(INFO) << "  Execution time: " << stats.timing().execution_time_ns() << " ns";
      }
    }

    for (const auto& agent_stats : exec_stats.agent_execution_stats()) {
      LOG(INFO) << "Agent Execution Stats:";
      LOG(INFO) << "  Execution time: " << agent_stats.execution_time_ns() << " ns";
      LOG(INFO) << "  Bytes processed: " << agent_stats.bytes_processed();
      LOG(INFO) << "  Records processed: " << agent_stats.records_processed();
    }
  }

  auto output_names = result_server.output_tables();
  if (!output_names.size()) {
    LOG(FATAL) << "Query produced no output tables.";
  }
  std::string output_name = *(result_server.output_tables().begin());
  LOG(INFO) << absl::Substitute("Writing results for output table: $0", output_name);
  // Write output table to CSV.
  TableToCsv(output_filename, result_server.query_results(output_name));
  return 0;
}
