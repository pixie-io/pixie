#include <fstream>
#include <iostream>
#include <string>

#include <parser.hpp>

#include "src/carnot/carnot.h"
#include "src/carnot/exec/table.h"
#include "src/common/env.h"
#include "src/shared/types/column_wrapper.h"
DEFINE_string(input_file, gflags::StringFromEnv("INPUT_FILE", ""),
              "The csv containing data to run the query on.");

DEFINE_string(output_file, gflags::StringFromEnv("OUTPUT_FILE", ""),
              "The file path to write the output data to.");

DEFINE_string(query, gflags::StringFromEnv("QUERY", ""), "The query to run.");

DEFINE_int64(rowbatch_size, gflags::Int64FromEnv("ROWBATCH_SIZE", 0),
             "The size of the rowbatches.");

/**
 * Gets the corresponding pl::DataType from the string type in the csv.
 * @param type the string from the csv.
 * @return the pl::DataType.
 */
pl::StatusOr<pl::types::DataType> GetTypeFromHeaderString(const std::string& type) {
  if (type == "int64") {
    return pl::types::INT64;
  } else if (type == "float64") {
    return pl::types::FLOAT64;
  } else if (type == "boolean") {
    return pl::types::BOOLEAN;
  } else if (type == "string") {
    return pl::types::STRING;
  } else if (type == "time64ns") {
    return pl::types::TIME64NS;
  } else {
    return pl::error::InvalidArgument("Could not recognize type from header.");
  }
}

/**
 * Takes the value in an arrow array and returns the string representation.
 * @param arr The arrow array.
 * @param idx The value's index in the arrow array.
 * @param type The pl::DataType of the value.
 * @return The string representation of the value, or an error.
 */
pl::StatusOr<std::string> GetValueAsString(arrow::Array* arr, int64_t idx,
                                           pl::types::DataType type) {
  switch (type) {
    case pl::types::INT64:
    case pl::types::TIME64NS:
      return absl::StrFormat("%d",
                             pl::carnot::udf::GetValue(static_cast<arrow::Int64Array*>(arr), idx));
    case pl::types::FLOAT64:
      return absl::StrFormat("%.2f",
                             pl::carnot::udf::GetValue(static_cast<arrow::DoubleArray*>(arr), idx));
    case pl::types::BOOLEAN:
      return absl::StrFormat(
          "%s", pl::carnot::udf::GetValue(static_cast<arrow::BooleanArray*>(arr), idx) == true
                    ? "true"
                    : "false");
    case pl::types::STRING:
      return absl::StrFormat("%s",
                             pl::carnot::udf::GetValue(static_cast<arrow::StringArray*>(arr), idx));
    default:
      return pl::error::InvalidArgument("Could not read output.");
  }
}

/**
 * Convert the csv at the given filename into a Carnot table.
 * @param filename The filename of the csv to convert.
 * @return The Carnot table.
 */
std::shared_ptr<pl::carnot::exec::Table> GetTableFromCsv(const std::string& filename,
                                                         int64_t rb_size) {
  std::ifstream f(filename);
  aria::csv::CsvParser parser(f);

  // The schema of the columns.
  std::vector<pl::types::DataType> types;
  // The names of the columns.
  std::vector<std::string> names;

  // Get the columns types and names.
  auto row_idx = 0;
  for (auto& row : parser) {
    auto col_idx = 0;
    for (auto& field : row) {
      if (row_idx == 0) {
        auto type = GetTypeFromHeaderString(field).ConsumeValueOrDie();
        // Currently reading the first row, which should be the types of the columns.
        types.push_back(type);
      } else if (row_idx == 1) {  // Reading second row, should be the names of columns.
        names.push_back(field);
      }
      col_idx++;
    }
    row_idx++;
    if (row_idx > 1) {
      break;
    }
  }

  // Construct the table.
  auto rel = pl::carnot::plan::Relation(types, names);
  auto table = std::make_shared<pl::carnot::exec::Table>(rel);

  // Add rowbatches to the table.
  row_idx = 0;
  std::unique_ptr<std::vector<pl::types::SharedColumnWrapper>> batch;
  for (auto& row : parser) {
    if (row_idx % rb_size == 0) {
      if (batch) {
        auto s = table->TransferRecordBatch(std::move(batch));
        if (!s.ok()) {
          LOG(ERROR) << "Couldn't add record batch to table.";
        }
      }

      // Create new batch.
      batch = std::make_unique<std::vector<pl::types::SharedColumnWrapper>>();
      // Create vectors for each column.
      for (auto type : types) {
        auto wrapper = pl::types::ColumnWrapper::Make(type, 0);
        batch->push_back(wrapper);
      }
    }
    auto col_idx = 0;
    for (auto& field : row) {
      switch (types[col_idx]) {
        case pl::types::DataType::INT64:
          dynamic_cast<pl::types::Int64ValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::stoi(field));
          break;
        case pl::types::DataType::FLOAT64:
          dynamic_cast<pl::types::Float64ValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(std::stof(field));
          break;
        case pl::types::DataType::BOOLEAN:
          dynamic_cast<pl::types::BoolValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(field == "true" ? true : false);
          break;
        case pl::types::DataType::STRING:
          dynamic_cast<pl::types::StringValueColumnWrapper*>(batch->at(col_idx).get())
              ->Append(absl::StrFormat("%s", field));
          break;
        case pl::types::DataType::TIME64NS:
          dynamic_cast<pl::types::Time64NSValueColumnWrapper*>(batch->at(col_idx).get())
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
void TableToCsv(const std::string& filename, pl::carnot::exec::Table* table) {
  std::ofstream output_csv;
  output_csv.open(filename);

  auto col_idxs = std::vector<int64_t>();
  for (int64_t i = 0; i < table->NumColumns(); i++) {
    col_idxs.push_back(i);
  }

  std::vector<std::string> output_col_names;
  for (size_t i = 0; i < col_idxs.size(); i++) {
    output_col_names.push_back(table->GetColumn(i)->name());
  }
  output_csv << absl::StrFormat("%s\n", absl::StrJoin(output_col_names, ","));

  for (auto i = 0; i < table->NumBatches(); i++) {
    auto rb = table->GetRowBatch(i, col_idxs, arrow::default_memory_pool()).ConsumeValueOrDie();
    for (auto row_idx = 0; row_idx < rb->num_rows(); row_idx++) {
      std::vector<std::string> row;
      for (size_t col_idx = 0; col_idx < col_idxs.size(); col_idx++) {
        auto val = GetValueAsString(rb->ColumnAt(col_idx).get(), row_idx,
                                    table->GetColumn(col_idx)->data_type());
        row.push_back(val.ConsumeValueOrDie());
      }
      output_csv << absl::StrFormat("%s\n", absl::StrJoin(row, ","));
    }
  }
  output_csv.close();
}

int main(int argc, char* argv[]) {
  pl::InitEnvironmentOrDie(&argc, argv);

  auto filename = FLAGS_input_file;
  auto output_filename = FLAGS_output_file;
  auto query = FLAGS_query;
  auto rb_size = FLAGS_rowbatch_size;

  auto table = GetTableFromCsv(filename, rb_size);

  // Execute query.
  pl::carnot::Carnot carnot;
  auto s = carnot.Init();
  if (!s.ok()) {
    LOG(FATAL) << "Carnot failed to init.";
  }
  auto table_store = carnot.table_store();
  table_store->AddTable("csv_table", table);
  auto exec_status = carnot.ExecuteQuery(query);
  auto res = exec_status.ConsumeValueOrDie();

  // Write output table to CSV.
  auto output_table = res.output_tables_[0];
  TableToCsv(output_filename, output_table);

  pl::ShutdownEnvironmentOrDie();
  return 0;
}
