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

#include <math.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sentencepiece/sentencepiece_processor.h>

#include <memory>
#include <string>
#include <vector>

#include "src/carnot/exec/ml/coreset.h"
#include "src/carnot/exec/ml/kmeans.h"
#include "src/carnot/exec/ml/sampling.h"
#include "src/carnot/exec/ml/transformer_executor.h"
#include "src/carnot/udf/model_executor.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

using exec::ml::CoresetDriver;
using exec::ml::CoresetTree;
using exec::ml::KMeans;
using exec::ml::KMeansCoreset;

int load_floats_from_json(std::string in, Eigen::VectorXf* out, int max_num);
std::string write_ints_to_json(int* arr, int num);

class TransformerUDF : public udf::ScalarUDF {
 public:
  TransformerUDF() : TransformerUDF("/embedding.proto") {}
  explicit TransformerUDF(std::string model_proto_path) : model_proto_path_(model_proto_path) {}
  StringValue Exec(FunctionContext* ctx, StringValue doc) {
    auto executor =
        ctx->model_pool()->GetModelExecutor<exec::ml::TransformerExecutor>(model_proto_path_);
    std::string output;
    executor->Execute(doc, &output);
    return output;
  }

 private:
  std::string model_proto_path_;
};

class SentencePieceUDF : public udf::ScalarUDF {
 public:
  SentencePieceUDF() : SentencePieceUDF("/sentencepiece.proto") {}
  explicit SentencePieceUDF(std::string model_proto_path) {
    const auto status = processor_.Load(model_proto_path);
    if (!status.ok()) {
      LOG(INFO) << "Failed to load processor: " << status.ToString();
    }
  }

  StringValue Exec(FunctionContext*, StringValue in) {
    std::vector<int> ids;
    processor_.Encode(in, &ids);
    return write_ints_to_json(ids.data(), ids.size());
  }

 private:
  sentencepiece::SentencePieceProcessor processor_;
};

class KMeansUDA : public udf::UDA {
 public:
  KMeansUDA() : KMeansUDA(64) {}
  explicit KMeansUDA(int d)
      : d_(d), coreset_(/*base_bucket_size*/ 64, d, /*r*/ 4, /*coreset_size*/ 64) {}
  void Update(FunctionContext*, StringValue in, Int64Value k) {
    if (k_ == -1) {
      k_ = k.val;
    }
    Eigen::VectorXf point(d_);
    int d = load_floats_from_json(in, &point, d_);
    DCHECK_EQ(d_, d);
    coreset_.Update(point);
  }
  void Merge(FunctionContext*, const KMeansUDA& other) { coreset_.Merge(other.coreset_); }
  StringValue Finalize(FunctionContext*) {
    auto point_set = coreset_.Query();
    KMeans kmeans(k_);
    kmeans.Fit(point_set);
    return kmeans.ToJSON();
  }

  StringValue Serialize(FunctionContext*) { return coreset_.ToJSON(); }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    coreset_.FromJSON(data);
    return Status::OK();
  }

 protected:
  int d_;
  int k_ = -1;
  CoresetDriver<CoresetTree<KMeansCoreset>> coreset_;
};

class KMeansUDF : public udf::ScalarUDF {
 public:
  KMeansUDF() : KMeansUDF(64) {}
  explicit KMeansUDF(int d) : d_(d) {}

  Int64Value Exec(FunctionContext*, StringValue embedding, StringValue kmeans_json) {
    if (kmeans_ == nullptr) {
      kmeans_ = std::make_unique<KMeans>(0);
      kmeans_->FromJSON(kmeans_json);
    }
    Eigen::VectorXf point(d_);
    int d = load_floats_from_json(embedding, &point, d_);
    DCHECK_EQ(d_, d);
    return kmeans_->Transform(point);
  }

 private:
  int d_;
  std::unique_ptr<KMeans> kmeans_;
};

template <typename TArg>
class ReservoirSampleUDA : public udf::UDA {
 public:
  ReservoirSampleUDA() : ReservoirSampleUDA(1) {}
  explicit ReservoirSampleUDA(size_t k) : k_(k), count_(0) {}
  void Update(FunctionContext*, TArg val) {
    count_++;
    if (reservoir_.size() < k_) {
      reservoir_.push_back(val);
      return;
    }
    auto i = exec::ml::randint(count_);
    if (i < k_) {
      reservoir_[i] = val;
    }
  }
  void Merge(FunctionContext*, const ReservoirSampleUDA<TArg>& other) {
    DCHECK_EQ(k_, other.k_);
    for (const auto& val : other.reservoir_) {
      if (reservoir_.size() < k_) {
        reservoir_.push_back(val);
        continue;
      }
      size_t i = exec::ml::randint(count_ + other.count_);
      if (i < other.count_) {
        reservoir_[i % k_] = val;
      }
    }
    count_ += other.count_;
  }
  TArg Finalize(FunctionContext*) { return reservoir_[0]; }

 private:
  size_t k_;
  size_t count_;
  std::vector<TArg> reservoir_;
};

void RegisterMLOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
