#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <sole.hpp>

#include "src/agent/throwaway_dummy_data.h"
#include "src/carnot/exec/table.h"
#include "src/common/status.h"
#include "src/shared/types/types.h"

using pl::carnot::exec::Column;
using pl::carnot::exec::RowDescriptor;
using pl::carnot::exec::Table;

using pl::types::DataType;
using pl::types::Float64Value;
using pl::types::Int64Value;
using pl::types::StringValue;
using pl::types::Time64NSValue;
using pl::types::ToArrow;

using std::string;

namespace pl {
namespace agent {

namespace {

std::string GetUserName(int index) {
  std::vector<std::string> user_names(
      {"Y@d.com", "S@a.com", "H@w.com", "X@s.com", "H@h.com", "I@y.com", "P@b.com", "U@g.com",
       "E@q.com", "K@n.com", "P@j.com", "J@t.com", "Y@u.com", "Z@n.com", "N@c.com", "C@y.com",
       "S@c.com", "Q@a.com", "K@w.com", "E@i.com", "C@p.com", "S@z.com", "J@k.com", "Z@q.com"});
  auto idx = index % user_names.size();
  return user_names[idx];
}

std::vector<std::string> tiers({"tier_0", "tier_1", "tier_2"});

size_t GetTierIndex(const string& user_name) {
  return std::hash<std::string>()(user_name) % tiers.size();
}
static std::string GetUserTier(const string& user_name) { return tiers[GetTierIndex(user_name)]; }

}  // namespace

StatusOr<std::shared_ptr<Table>> FakeHipsterTable() {
  std::random_device rdev;   // only used once to initialise (seed) engine
  std::mt19937 rng(rdev());  // random-number engine used (Mersenne-Twister in this case)
  std::uniform_int_distribution<int> uniform_dist(0, 1000);
  std::exponential_distribution<> response(1.0);
  std::exponential_distribution<> cart_size_dist(1.0);

  // Distributions to use to for latency. Latency depends on 3 parameters:
  // 1. Base latency (normal dist.)
  // 2. Cart size: Larger carts => more latency
  // 3. User tier.

  std::normal_distribution<> latency_dist(100, 50);
  std::exponential_distribution<> latency_cart_size(2.0);
  std::exponential_distribution<> latency_user_tier(4.0);
  std::exponential_distribution<> time_latency(1.0);

  std::normal_distribution<> time_interval(1.0, 0.1);

  // Schema:
  // Time stamp (ms), Transaction id, Http request, Latency, htttp response code (200 or 500)
  auto descriptor = std::vector<DataType>(
      {DataType::TIME64NS, DataType::STRING, DataType::STRING, DataType::FLOAT64, DataType::INT64});
  RowDescriptor rd = RowDescriptor(descriptor);

  const int num_records = 6 * 60 * 60;             // Data for 6 hours (one point per second)
  const int64_t start_time = 1551668293000000000;  // 3/3/2019, 6:58:13 PM.
  const int64_t time_interval_mult = 1e9;          // 1 sec.
  const int64_t batch_size = 4096;
  int bad_start = num_records / 3;
  int bad_stop = bad_start + num_records / 10;

  auto time_col = std::make_shared<Column>(DataType::TIME64NS, "time_");
  auto transaction_id_col = std::make_shared<Column>(DataType::STRING, "transaction_id");
  auto http_request_col = std::make_shared<Column>(DataType::STRING, "http_request");
  auto latency_col = std::make_shared<Column>(DataType::FLOAT64, "latency_ms");
  auto http_response_col = std::make_shared<Column>(DataType::INT64, "http_response");

  std::vector<Time64NSValue> time_col_chunk;
  std::vector<StringValue> transaction_id_chunk;
  std::vector<StringValue> http_request_chunk;
  std::vector<Float64Value> latency_col_chunk;
  std::vector<Int64Value> http_response_chunk;

  int64_t batch_idx = 0;
  int64_t current_time = start_time;
  for (int i = 0; i < num_records; ++i) {
    int interval = std::min(time_interval_mult * 2,
                            std::max(time_interval_mult / 2,
                                     time_interval_mult * static_cast<int>(time_interval(rng))));
    current_time += interval;
    time_col_chunk.emplace_back(Time64NSValue(current_time));
    auto transaction_id = StringValue(sole::uuid4().str());
    transaction_id_chunk.emplace_back(transaction_id);
    auto user_name = GetUserName(uniform_dist(rng));
    int cart_size = std::min(60, std::max(1, 10 * static_cast<int>(cart_size_dist(rng))));

    // latency consists of three parameters.
    // 1. Normally distributed base latency.
    // 2. exponentially distributed offset based on cart size.
    // 3. exponentially distributed offset based on user tier.
    auto latency = latency_dist(rng) + (5 * cart_size * latency_cart_size(rng)) +
                   10 * ((std::pow(GetTierIndex(user_name), 4)) * latency_user_tier(rng));
    // Also introduce cases where latency is impacted by bad starts and stops.
    if (i > bad_start && i < bad_stop) {
      latency += 100 * time_latency(rng);
    }
    latency = std::max(10.0, latency);

    latency_col_chunk.emplace_back(latency);
    http_response_chunk.emplace_back((response(rng) < 2.5) ? 200 : 500);
    // JSON http request
    auto req = StringValue(absl::Substitute(
        "{\"req_id\": \"$0\", \"cart_size\": $1, \"cart_data\": \"hipster brand items\", "
        "\"user_info\" "
        ": { \"user_name\": \"$2\", \"user_type\": \"$3\" } }",
        transaction_id, cart_size, user_name, GetUserTier(user_name)));

    http_request_chunk.emplace_back(req);
    batch_idx++;
    if (batch_idx == batch_size) {
      PL_RETURN_IF_ERROR(time_col->AddBatch(ToArrow(time_col_chunk, arrow::default_memory_pool())));
      PL_RETURN_IF_ERROR(transaction_id_col->AddBatch(
          ToArrow(transaction_id_chunk, arrow::default_memory_pool())));
      PL_RETURN_IF_ERROR(
          http_request_col->AddBatch(ToArrow(http_request_chunk, arrow::default_memory_pool())));
      PL_RETURN_IF_ERROR(
          latency_col->AddBatch(ToArrow(latency_col_chunk, arrow::default_memory_pool())));
      PL_RETURN_IF_ERROR(
          http_response_col->AddBatch(ToArrow(http_response_chunk, arrow::default_memory_pool())));
      time_col_chunk.clear();
      transaction_id_chunk.clear();
      http_request_chunk.clear();
      latency_col_chunk.clear();
      http_response_chunk.clear();
      batch_idx = 0;
    }
  }

  auto table = std::make_shared<Table>(rd);
  PL_RETURN_IF_ERROR(table->AddColumn(time_col));
  PL_RETURN_IF_ERROR(table->AddColumn(transaction_id_col));
  PL_RETURN_IF_ERROR(table->AddColumn(http_request_col));
  PL_RETURN_IF_ERROR(table->AddColumn(latency_col));
  PL_RETURN_IF_ERROR(table->AddColumn(http_response_col));

  return table;
}

}  // namespace agent
}  // namespace pl
