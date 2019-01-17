#include <benchmark/benchmark.h>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

#include "src/data_collector/proto/canonical_message.pb.h"

std::vector<std::tuple<int64_t, double, int64_t>> CreateData(int size) {
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
  ::pl::canonical_message::CanonincalStream canonical_stream;
  size_t data_size = collector_data.size();
  for (size_t i = 0; i < data_size; ++i) {
    ::pl::canonical_message::Canonical* canonical_data = canonical_stream.add_data_stream();
    canonical_data->set_time_stamp(std::get<0>(collector_data[i]));
    canonical_data->set_data1(std::get<1>(collector_data[i]));
    canonical_data->set_data2(std::get<2>(collector_data[i]));
  }

  // Serialize the proto message to a char array
  int message_size = canonical_stream.ByteSize();
  auto serialized_message = std::make_unique<char[]>(message_size);
  canonical_stream.SerializeToArray(serialized_message.get(), message_size);

  // Deserialize proto message
  ::pl::canonical_message::CanonincalStream recvd_stream;
  recvd_stream.ParseFromArray(serialized_message.get(), message_size);

  // Extract data from canonical message to table format for engine.
  std::vector<int64_t> time_stamp(collector_data.size());
  std::vector<int64_t> data_1(collector_data.size());
  std::vector<double> data_0(collector_data.size());

  int num_items = recvd_stream.data_stream_size();

  for (int i = 0; i < num_items; ++i) {
    ::pl::canonical_message::Canonical datum = recvd_stream.data_stream(i);
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
  ::pl::canonical_message::CanonicalRepeatedColumn canonical_stream;
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
  ::pl::canonical_message::CanonicalRepeatedColumn recvd_stream;
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

// Benchmark to measure performance for converting data to
// a column of std::vector<T> for each column.
static void BM_raw_struct_vector(benchmark::State& state) {  // NOLINT
  auto collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = RawStructProcessDataVector(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
}

// Benchmark to measure performance for converting data to
// a column of arrays using dynamic memory allocation and unique pointers.
static void BM_raw_struct_dynamic_array(benchmark::State& state) {  // NOLINT
  auto collected_data = CreateData(state.range(0));
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
static void BM_proto(benchmark::State& state) {  // NOLINT
  auto collected_data = CreateData(state.range(0));
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
static void BM_proto_repeated_column(benchmark::State& state) {  // NOLINT
  auto collected_data = CreateData(state.range(0));
  for (auto _ : state) {
    auto res = CanonicalProtoProcessDataRepeatedColumn(collected_data);
    benchmark::DoNotOptimize(res);
  }
  state.SetBytesProcessed(state.iterations() * ((2 * sizeof(int64_t)) + sizeof(double)) *
                          collected_data.size());
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
