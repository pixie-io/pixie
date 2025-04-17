#include "src/vizier/services/agent/shared/tetragonreader/tetragon_reader.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace px {
namespace vizier {
namespace services {
namespace tetragonreader {

TetragonReader::TetragonReader(table_store::TableStore* table_store)
    : table_store_(table_store) {
  px::types::Relation rel({
      types::ColumnWrapper("ts", types::DataType::TIME64NS),
      types::ColumnWrapper("event_type", types::DataType::STRING),
      types::ColumnWrapper("json", types::DataType::STRING),
  });

  LOG(INFO) << "TetragonReader: Creating tetragon_logs table.";
  tetragon_table_ = table_store::Table::Create("tetragon_logs", rel);
  table_store_->AddTable(tetragon_table_, "tetragon_logs");
}

TetragonReader::~TetragonReader() {
  Stop();
}

void TetragonReader::Start() {
  if (running_) return;
  LOG(INFO) << "TetragonReader: Starting reader thread.";
  running_ = true;
  reader_thread_ = std::thread(&TetragonReader::ReaderThread, this);
}

void TetragonReader::Stop() {
  if (!running_) return;
  LOG(INFO) << "TetragonReader: Stopping reader thread.";
  running_ = false;
  if (reader_thread_.joinable()) {
    reader_thread_.join();
  }
}

void TetragonReader::ReaderThread() {
  std::ifstream file("/var/run/cilium/tetragon/tetragon.log");
  if (!file.is_open()) {
    LOG(ERROR) << "TetragonReader: Failed to open Tetragon log file.";
    return;
  }

  LOG(INFO) << "TetragonReader: Reading from Tetragon log file.";
  std::string line;
  while (running_ && std::getline(file, line)) {
    LOG(INFO) << "TetragonReader: Read line from log.";
    Ingest(line);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  LOG(INFO) << "TetragonReader: Reader thread exiting.";
  file.close();
}

void TetragonReader::Ingest(const std::string& json_line) {
  if (!tetragon_table_) return;

  json j;
  try {
    j = json::parse(json_line);
  } catch (const std::exception& e) {
    LOG(ERROR) << "TetragonReader: Failed to parse JSON line: " << e.what();
    return;
  }

  std::string time_str = j.value("time", "");
  int64_t ts = 0;
  if (!time_str.empty()) {
    auto tp = px::chrono::RFC3339ToTime(time_str);
    if (tp.ok()) {
      ts = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.ConsumeValueOrDie().time_since_epoch()).count();
    }
  }

  auto rb = std::make_unique<types::ColumnWrapperRecordBatch>(tetragon_table_->schema(), 1);

  rb->Append<types::TIME64NS>(0, ts);
  rb->Append<types::STRING>(1, j.value("function_name", ""));
  rb->Append<types::INT64>(2, j.value("pid", 0));
  rb->Append<types::INT64>(3, j.value("uid", 0));
  rb->Append<types::STRING>(4, j.value("binary", ""));
  rb->Append<types::STRING>(5, j.value("args", ""));
  rb->Append<types::STRING>(6, j.value("policy_name", ""));

  LOG(INFO) << "TetragonReader: Ingested event for function: " << j.value("function_name", "") << ", pid: " << j.value("pid", 0);

  tetragon_table_->WriteRowBatch(std::move(rb));
}

}  // namespace tetragonreader
}  // namespace services
}  // namespace vizier
}  // namespace px
