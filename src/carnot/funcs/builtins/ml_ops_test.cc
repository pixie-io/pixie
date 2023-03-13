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

#include <gflags/gflags.h>
#include <gmock/gmock.h>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/carnot/funcs/builtins/ml_ops.h"
#include "src/carnot/udf/test_utils.h"
#include "src/common/base/base.h"

#include "src/carnot/exec/ml/eigen_test_utils.h"

DEFINE_string(sentencepiece_dir, "", "Path to sentencepiece.proto");
DEFINE_string(embedding_dir, "", "Path to embedding.proto");

namespace px {
namespace carnot {
namespace builtins {

using ::px::carnot::udf::FunctionContext;

std::string write_vector_to_json(const Eigen::VectorXf& vector) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartArray();
  for (int i = 0; i < vector.rows(); i++) {
    writer.Double(vector(i));
  }
  writer.EndArray();
  return sb.GetString();
}

TEST(KMeans, basic) {
  int k = 3;
  int d = 2;

  auto kmeans_uda_tester = udf::UDATester<KMeansUDA>(d);

  Eigen::MatrixXf expected_centroids = kmeans_expected_centroids();
  Eigen::MatrixXf points = kmeans_test_data();

  for (int i = 0; i < points.rows(); i++) {
    auto inp = write_vector_to_json(points(i, Eigen::indexing::all).transpose());
    kmeans_uda_tester.ForInput(inp, k);
  }

  auto res = kmeans_uda_tester.Result();
  px::carnot::exec::ml::KMeans kmeans(k);
  kmeans.FromJSON(res);
  EXPECT_THAT(kmeans.centroids(), UnorderedRowsAre(expected_centroids, 0.1));
}

TEST(SentencePiece, basic) {
  auto udf_tester = udf::UDFTester<SentencePieceUDF>(FLAGS_sentencepiece_dir);
  udf_tester.ForInput("Test 123!");
  // This test is just a sanity check to see that the sentencepiece UDF runs.
  // If the model changes this test will almost certainly fail.
  udf_tester.Expect("[4,197,803,195,16,5001]");
}

TEST(Transformer, basic) {
  auto pool = udf::ModelPool::Create();
  auto ctx = std::make_unique<FunctionContext>(nullptr, pool.get());
  auto udf_tester = udf::UDFTester<TransformerUDF>(std::move(ctx), FLAGS_embedding_dir);
  udf_tester.ForInput("[4,197,803,195,16,5001]");
  // This test is just a sanity check to see that the transformer UDF runs.
  // If the model changes this test will fail.
  auto vector_str = udf_tester.Result();
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(vector_str.data());
  ASSERT_NE(ok, nullptr);
  std::vector<double> vals;
  for (rapidjson::Value::ConstValueIterator itr = d.Begin(); itr != d.End(); ++itr) {
    vals.push_back(itr->GetFloat());
  }
  std::vector<double> expected_vals = {8.423064231872559, 1.762765645980835, 17.635025024414064,
                                       15.878694534301758};
  // Sanity check the model by checking the first few values of the model output.
  for (const auto& [i, val] : Enumerate(expected_vals)) {
    EXPECT_NEAR(val, vals[i], 0.0001);
  }
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
