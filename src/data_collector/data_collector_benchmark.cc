#include <benchmark/benchmark.h>

#include <arrow/api.h>
#include <arrow/io/memory.h>
#include <arrow/ipc/reader.h>
#include <arrow/ipc/writer.h>

#include <glog/logging.h>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include "src/data_collector/proto/canonical_message.pb.h"

using arrow::DoubleBuilder;
using arrow::Int64Builder;
using pl::datacollector::datacollectorpb::Canonical;
using pl::datacollector::datacollectorpb::CanonicalRepeatedColumn;
using pl::datacollector::datacollectorpb::CanonicalStream;

// Copied from third_party/arrow/cpp/src/arrow/test-util.h
// Including test-util.h causes unnecessary dependencies
#define ARROW_ABORT_NOT_OK(s)            \
  do {                                   \
    ::arrow::Status _s = (s);            \
    if (ARROW_PREDICT_FALSE(!_s.ok())) { \
      LOG(FATAL) << s.ToString();        \
    }                                    \
  } while (false);

auto CreateData(int size) {
  std::vector<std::tuple<int64_t, double, int64_t>> data(size);

  std::mt19937 rng(2);  // Seed with the same value for reproducibility.
  std::uniform_real_distribution<double> d_dist(0.0, 42.0);
  std::uniform_int_distribution<int64_t> i_dist(0, 42000);
  auto gen = [&d_dist, &i_dist, &rng]() {
    return std::make_tuple(i_dist(rng), d_dist(rng), i_dist(rng));
  };
  std::generate(data.begin(), data.end(), gen);
  return data;
}

auto CreateArrowRecordBatch(size_t size) {
  // Random number distributions for generating data

  std::mt19937 rng(2);  // Seed with the same value for reproducibility.
  std::uniform_real_distribution<double> d_dist(0.0, 42.0);
  std::uniform_int_distribution<int64_t> i_dist(0, 42000);

  // Populate data into Arrow builders.
  arrow::MemoryPool* pool = arrow::default_memory_pool();

  Int64Builder time_stamp_builder(pool);
  DoubleBuilder data0_builder(pool);
  Int64Builder data1_builder(pool);

  // For performance, reserve the required amount of buffer up-front
  // Then use UnsafeAppend below.
  // Update: In practice, doesn't seem to make a performance difference
  // for BM_arrow benchmark.
  ARROW_ABORT_NOT_OK(time_stamp_builder.Reserve(size));
  ARROW_ABORT_NOT_OK(data0_builder.Reserve(size));
  ARROW_ABORT_NOT_OK(data1_builder.Reserve(size));

  for (size_t i = 0; i < size; ++i) {
    time_stamp_builder.UnsafeAppend(i_dist(rng));
    data0_builder.UnsafeAppend(d_dist(rng));
    data1_builder.UnsafeAppend(i_dist(rng));
  }

  // Finish building arrow Arrays.
  std::shared_ptr<arrow::Array> time_stamp_array;
  ARROW_ABORT_NOT_OK(time_stamp_builder.Finish(&time_stamp_array));

  std::shared_ptr<arrow::Array> data0_array;
  ARROW_ABORT_NOT_OK(data0_builder.Finish(&data0_array));

  std::shared_ptr<arrow::Array> data1_array;
  ARROW_ABORT_NOT_OK(data1_builder.Finish(&data1_array));

  // Set Arrow Schema.
  std::vector<std::shared_ptr<arrow::Field>> schema_vector = {
      arrow::field("time_stamp", arrow::int64()), arrow::field("data0", arrow::float64()),
      arrow::field("data1", arrow::int64())};
  auto schema = std::make_shared<arrow::Schema>(schema_vector);
  // Create Arrow Record Batch.
  auto record_batch =
      arrow::RecordBatch::Make(schema, size, {time_stamp_array, data0_array, data1_array});

  return record_batch;
}

// This function converts a vector of tuples of data into
// columnar vectors to simulate conversion of data from
// data agent to a format that the pixie agent can use.
// For simplicity, we are not modeling the pipe in between
// to transfer data.
auto RawStructProcessDataVector(
    const std::vector<std::tuple<int64_t, double, int64_t>>& collector_data) {
  std::vector<int64_t> time_stamp(collector_data.size());
  std::vector<int64_t> data_1(collector_data.size());
  std::vector<double> data_0(collector_data.size());

  size_t data_size = collector_data.size();
  // Convert tuples to columns
  for (size_t i = 0; i < data_size; ++i) {
    time_stamp[i] = (std::get<0>(collector_data[i]));
    data_0[i] = (std::get<1>(collector_data[i]));
    data_1[i] = (std::get<2>(collector_data[i]));
  }
  return std::make_tuple(time_stamp, data_0, data_1);
}

// Convert a vector of tuples of data into
// columnar arrays of data by dynamically
// allocating memory using unique pointers.
auto RawStructProcessDataDynamicArray(
    const std::vector<std::tuple<int64_t, double, int64_t>>& collector_data) {
  size_t data_size = collector_data.size();
  auto time_stamp = std::make_unique<int64_t[]>(data_size);
  auto data_0 = std::make_unique<double[]>(data_size);
  auto data_1 = std::make_unique<int64_t[]>(data_size);

  // Convert tuples to columns
  for (size_t i = 0; i < data_size; ++i) {
    time_stamp[i] = (std::get<0>(collector_data[i]));
    data_0[i] = (std::get<1>(collector_data[i]));
    data_1[i] = (std::get<2>(collector_data[i]));
  }
  return std::make_tuple(std::move(time_stamp), std::move(data_0), std::move(data_1));
}

// Convert a vector of tuples of data into a proto message.
// The proto message consists of a datum struct which is then
// sent as a repeated message. Deserialize the message and convert to
// columnar vectors.
auto CanonicalProtoProcessData(
    const std::vector<std::tuple<int64_t, double, int64_t>>& collector_data) {
  // Create a  canonical proto message
  CanonicalStream canonical_stream;
  size_t data_size = collector_data.size();
  for (size_t i = 0; i < data_size; ++i) {
    auto* canonical_data = canonical_stream.add_data_stream();
    canonical_data->set_time_stamp(std::get<0>(collector_data[i]));
    canonical_data->set_data1(std::get<1>(collector_data[i]));
    canonical_data->set_data2(std::get<2>(collector_data[i]));
  }

  // Serialize the proto message to a char array
  int64_t message_size = canonical_stream.ByteSizeLong();
  auto serialized_message = std::make_unique<char[]>(message_size);
  canonical_stream.SerializeToArray(serialized_message.get(), message_size);

  // Deserialize proto message
  CanonicalStream recvd_stream;
  recvd_stream.ParseFromArray(serialized_message.get(), message_size);

  // Extract data from canonical message to table format for engine.
  std::vector<int64_t> time_stamp(collector_data.size());
  std::vector<int64_t> data_1(collector_data.size());
  std::vector<double> data_0(collector_data.size());

  int num_items = recvd_stream.data_stream_size();

  for (int i = 0; i < num_items; ++i) {
    Canonical datum = recvd_stream.data_stream(i);
    time_stamp[i] = datum.time_stamp();
    data_0[i] = datum.data1();
    data_1[i] = datum.data2();
  }
  return std::make_tuple(time_stamp, data_0, data_1);
}

// Convert a vector of tuples of data into a proto message.
// Instead of packing a datum into one message, each component
// is sent as a repeated message. Note that with this scheme,
// the data collector has to ensure time alignment of the data.
// Deserialize the message and convert to columnar vectors.
auto CanonicalProtoProcessDataRepeatedColumn(
    const std::vector<std::tuple<int64_t, double, int64_t>>& collector_data) {
  // Create a  canonical other proto message
  CanonicalRepeatedColumn canonical_stream;
  size_t data_size = collector_data.size();
  for (size_t i = 0; i < data_size; ++i) {
    canonical_stream.add_time_stamp(std::get<0>(collector_data[i]));
    canonical_stream.add_data1(std::get<1>(collector_data[i]));
    canonical_stream.add_data2(std::get<2>(collector_data[i]));
  }

  // Serialize the proto message to a void* array
  int64_t message_size = canonical_stream.ByteSizeLong();
  auto serialized_message = std::make_unique<char[]>(message_size);
  canonical_stream.SerializeToArray(serialized_message.get(), message_size);

  // Deserialize proto message
  CanonicalRepeatedColumn recvd_stream;
  recvd_stream.ParseFromArray(serialized_message.get(), message_size);

  // Extract data from canonical message to table format for engine.
  std::vector<int64_t> time_stamp(collector_data.size());
  std::vector<int64_t> data_1(collector_data.size());
  std::vector<double> data_0(collector_data.size());

  int num_items = recvd_stream.time_stamp_size();
  for (int i = 0; i < num_items; ++i) {
    time_stamp[i] = recvd_stream.time_stamp(i);
  }

  num_items = recvd_stream.data1_size();
  for (int i = 0; i < num_items; ++i) {
    data_0[i] = recvd_stream.data1(i);
  }

  num_items = recvd_stream.data2_size();
  for (int i = 0; i < num_items; ++i) {
    data_1[i] = recvd_stream.data2(i);
  }

  return std::make_tuple(time_stamp, data_0, data_1);
}

// Serialize and then deserialize an Arrow Record Batch.
// Used to model the overheads of sending messages with Arrow format.
auto ArrowProcessRecordBatch(const std::shared_ptr<arrow::RecordBatch> record_batch) {
  arrow::MemoryPool* pool = arrow::default_memory_pool();

  // Serialize Record Batch (flatbuffers under the hood).
  int64_t size = 0;
  ARROW_ABORT_NOT_OK(arrow::ipc::GetRecordBatchSize(*record_batch, &size));

  std::shared_ptr<arrow::Buffer> buffer;
  ARROW_ABORT_NOT_OK(AllocateBuffer(pool, size, &buffer));
  arrow::io::FixedSizeBufferWriter stream(buffer);

  // Main serialization call - does a memcpy under the hood.
  ARROW_ABORT_NOT_OK(
      arrow::ipc::SerializeRecordBatch(*record_batch, arrow::default_memory_pool(), &stream));

  // De-serialize (read) the Record Batch.
  std::shared_ptr<arrow::RecordBatch> result;
  arrow::io::BufferReader reader(buffer);

  // Main de-serialization call - uses a std::move, so no actualy copying!
  ARROW_ABORT_NOT_OK(arrow::ipc::ReadRecordBatch(record_batch->schema(), &reader, &result));

  // NOTE: Could send schema as well, but not modeling this as we are interested in the data
  // transfer
  //    Status SerializeSchema(const Schema& schema, MemoryPool* pool,std::shared_ptr<Buffer>* out);
  //    Status ReadSchema(io::InputStream* stream, std::shared_ptr<Schema>* out);

  return result;
}

// Convert a vector of tuples into an Arrow Record Batch
// Then serialize and de-serialize to model the data transfer.
// This is not efficient because the create of a Record Batch is slow.
auto ArrowProcessData(const std::vector<std::tuple<int64_t, double, int64_t>>& collector_data) {
  std::shared_ptr<arrow::RecordBatch> record_batch;
  record_batch = CreateArrowRecordBatch(collector_data.size());

  std::shared_ptr<arrow::RecordBatch> result;
  result = ArrowProcessRecordBatch(record_batch);

  return result;
}

// Benchmark to measure performance for converting data to
// a column of std::vector<T> for each column.
// NOLINTNEXTLINE(runtime/references)
static void BM_raw_struct_vector(benchmark::State& state) {
  const auto& collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = RawStructProcessDataVector(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
}

// Benchmark to measure performance for converting data to
// a column of arrays using dynamic memory allocation and unique pointers.
// NOLINTNEXTLINE(runtime/references)
static void BM_raw_struct_dynamic_array(benchmark::State& state) {
  const auto& collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = RawStructProcessDataDynamicArray(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
}

// Benchmark to measure performance for converting data to
// a proto message with repeated tuples of data
// and then converting that message to columnar vectors.
// NOLINTNEXTLINE(runtime/references)
static void BM_proto(benchmark::State& state) {
  const auto& collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = CanonicalProtoProcessData(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
}

// Benchmark to measure performance for converting data to
// a proto message with repeated individual components of the data
// and then converting that message to columnar vectors.
// NOLINTNEXTLINE(runtime/references)
static void BM_proto_repeated_column(benchmark::State& state) {
  const auto& collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = CanonicalProtoProcessDataRepeatedColumn(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
}

// Benchmark to measure performance for converting data to an
// Arrow record batch, and then serializing into a message and
// then deserializing.
// Performance sucks, because of the initial conversion.
// NOLINTNEXTLINE(runtime/references)
static void BM_arrow(benchmark::State& state) {
  size_t size = state.range(0);

  // Create the raw data: a vector of tuples.
  const auto& collected_data = CreateData(size);

  for (auto _ : state) {
    auto res = ArrowProcessData(collected_data);
    benchmark::DoNotOptimize(res);
  }

  state.SetBytesProcessed(state.iterations() * (2 * sizeof(int64_t) + sizeof(double)) * size);
}

// Benchmark to measure performance of serializing and deserializing
// Arrow record batch.
// NOLINTNEXTLINE(runtime/references)
static void BM_arrow_record_batch(benchmark::State& state) {
  size_t size = state.range(0);

  // Create the raw data: an arrow record batch of arrow arrays.
  const auto& record_batch = CreateArrowRecordBatch(size);

  for (auto _ : state) {
    auto res = ArrowProcessRecordBatch(record_batch);
    benchmark::DoNotOptimize(res);
  }

  state.SetBytesProcessed(state.iterations() * (2 * sizeof(int64_t) + sizeof(double)) * size);
}

const size_t kRangeMultiplier = 10;
const size_t kRangeBegin = 10;
const size_t kRangeEnd = 1000000;

BENCHMARK(BM_raw_struct_vector)->RangeMultiplier(kRangeMultiplier)->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_raw_struct_dynamic_array)
    ->RangeMultiplier(kRangeMultiplier)
    ->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_proto)->RangeMultiplier(kRangeMultiplier)->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_proto_repeated_column)
    ->RangeMultiplier(kRangeMultiplier)
    ->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_arrow)->RangeMultiplier(kRangeMultiplier)->Range(kRangeBegin, kRangeEnd);
BENCHMARK(BM_arrow_record_batch)->RangeMultiplier(kRangeMultiplier)->Range(kRangeBegin, kRangeEnd);
