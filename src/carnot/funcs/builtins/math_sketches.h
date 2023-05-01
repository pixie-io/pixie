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
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <utility>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/common/base/error.h"
#include "src/shared/types/types.h"
#include "tdigest/tdigest.h"

namespace px {
namespace carnot {
namespace builtins {

void WriteCentroidArray(rapidjson::Writer<rapidjson::StringBuffer>* writer,
                        const std::vector<tdigest::Centroid>& centroids);

std::vector<tdigest::Centroid> CentroidArrayFromJSON(const rapidjson::Value& val);

// TODO(zasgar): PL-419 Replace this when we add support for structs.
template <typename TArg>
class QuantilesUDA : public udf::UDA {
 public:
  QuantilesUDA() : digest_(1000) {}
  void Update(FunctionContext*, TArg val) { digest_.add(val.val); }
  void Merge(FunctionContext*, const QuantilesUDA& other) { digest_.merge(&other.digest_); }

  StringValue Finalize(FunctionContext*) {
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("p01", digest_.quantile(0.01), d.GetAllocator());
    d.AddMember("p10", digest_.quantile(0.10), d.GetAllocator());
    d.AddMember("p25", digest_.quantile(0.25), d.GetAllocator());
    d.AddMember("p50", digest_.quantile(0.50), d.GetAllocator());
    d.AddMember("p75", digest_.quantile(0.75), d.GetAllocator());
    d.AddMember("p90", digest_.quantile(0.90), d.GetAllocator());
    d.AddMember("p99", digest_.quantile(0.99), d.GetAllocator());
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);
    return sb.GetString();
  }

  static constexpr char kProcessedKey[] = "0";
  static constexpr char kUnprocessedKey[] = "1";
  static constexpr char kCompressionKey[] = "2";
  static constexpr char kMaxUnprocessedKey[] = "3";
  static constexpr char kMaxProcessedKey[] = "4";

  StringValue Serialize(FunctionContext*) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.Key(kProcessedKey);
    WriteCentroidArray(&writer, digest_.processed());
    writer.Key(kUnprocessedKey);
    WriteCentroidArray(&writer, digest_.unprocessed());
    writer.Key(kCompressionKey);
    writer.Double(digest_.compression());
    writer.Key(kMaxUnprocessedKey);
    writer.Uint64(digest_.maxUnprocessed());
    writer.Key(kMaxProcessedKey);
    writer.Uint64(digest_.maxProcessed());
    writer.EndObject();
    return sb.GetString();
  }

  Status Deserialize(FunctionContext*, const StringValue& json) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(json.data());
    if (ok == nullptr) {
      return error::InvalidArgument("invalid serialized tdigest");
    }
    auto processed = CentroidArrayFromJSON(d[kProcessedKey]);
    auto unprocessed = CentroidArrayFromJSON(d[kUnprocessedKey]);
    auto compression = d[kCompressionKey].GetDouble();
    auto maxUnprocessed = d[kMaxUnprocessedKey].GetUint64();
    auto maxProcessed = d[kMaxProcessedKey].GetUint64();
    digest_ = tdigest::TDigest(std::move(processed), std::move(unprocessed), compression,
                               maxUnprocessed, maxProcessed);
    return Status::OK();
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<QuantilesUDA>(types::ST_QUANTILES, {types::ST_NONE}),
            udf::ExplicitRule::Create<QuantilesUDA>(types::ST_DURATION_NS_QUANTILES,
                                                    {types::ST_DURATION_NS})};
  }

  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Approximates the distribution of the aggregated data.")
        .Details(
            "Calculates several useful percentiles of the aggregated data using "
            "[tdigest](https://github.com/tdunning/t-digest). Returns a serialized JSON object "
            "with the "
            "keys for 1%, 10%, 50%, 90%, and 99%. You can use `px.pluck_float64` to grab the "
            "specific values from the result.")
        .Example(R"doc(
        | # Calculate the quantiles.
        | df = df.agg(latency_dist=('latency_ms', px.quantiles))
        | # Pluck p99 from the quantiles.
        | df.p99 = px.pluck_float64(df.latency_dist, 'p99')
        )doc")
        .Arg("val", "The data to calculate the quantiles distribution.")
        .Returns("The quantiles data, serialized as a JSON dictionary.");
  }

 protected:
  tdigest::TDigest digest_;
};

void RegisterMathSketchesOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
