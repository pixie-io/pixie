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

#include "src/carnot/exec/ml/kmeans.h"
#include <random>

#include "src/carnot/exec/ml/sampling.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

void KMeans::Fit(std::shared_ptr<WeightedPointSet> set) {
  if (set->size() < 2) {
    LOG(ERROR) << "Fitting KMeans on less than 2 points is currently unsupported.";
    return;
  }
  auto points = set->points();
  auto weights = set->weights();

  centroids_.resize(k_, points.cols());
  switch (init_type_) {
    case KMeans::kKMeansPlusPlus:
      KMeansPlusPlusInit(points, weights);
  }

  int iter_count = 0;
  bool changed = true;
  while (iter_count < max_iters_ && changed) {
    changed = LloydsIteration(points, weights);
    iter_count++;
  }
}

bool KMeans::LloydsIteration(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights) {
  Eigen::MatrixXf new_centroids = Eigen::MatrixXf::Zero(centroids_.rows(), centroids_.cols());
  Eigen::ArrayXf centroid_weights = Eigen::ArrayXf::Zero(centroids_.rows());

  for (int i = 0; i < points.rows(); i++) {
    Eigen::VectorXf::Index closest_centroid;
    (centroids_.rowwise() - points(i, Eigen::indexing::all))
        .rowwise()
        .squaredNorm()
        .minCoeff(&closest_centroid);
    new_centroids(closest_centroid, Eigen::indexing::all) +=
        weights(i) * points(i, Eigen::indexing::all);
    centroid_weights(closest_centroid) += weights(i);
  }

  for (int i = 0; i < k_; i++) {
    if (centroid_weights[i] == 0.0f) {
      new_centroids(i, Eigen::indexing::all) = centroids_(i, Eigen::indexing::all);
      centroid_weights(i) = 1.0f;
    }
  }

  new_centroids = (new_centroids.array().colwise() / centroid_weights).matrix();
  if (new_centroids.isApprox(centroids_)) {
    return false;
  }
  centroids_ = new_centroids;
  return true;
}

void KMeans::KMeansPlusPlusInit(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights) {
  std::uniform_int_distribution<> dist(0, points.rows() - 1);
  auto firstCentroid = dist(random_gen_);
  centroids_(0, Eigen::indexing::all) = points(firstCentroid, Eigen::indexing::all);

  Eigen::VectorXf probDist(points.rows());
  for (auto i = 1; i < k_; i++) {
    for (auto j = 0; j < probDist.rows(); j++) {
      auto point = points(j, Eigen::indexing::all);
      Eigen::VectorXf::Index closestCentroid;
      auto dist = (centroids_(Eigen::seq(0, i - 1), Eigen::indexing::all).rowwise() - point)
                      .rowwise()
                      .squaredNorm()
                      .minCoeff(&closestCentroid);
      probDist(j) = weights(j) * dist;
    }
    std::discrete_distribution<> pointDist(probDist.begin(), probDist.end());
    auto ind = pointDist(random_gen_);
    centroids_(i, Eigen::indexing::all) = points(ind, Eigen::indexing::all);
  }
}

size_t KMeans::Transform(const Eigen::VectorXf& point) {
  size_t closest_centroid;
  (centroids_.rowwise() - point.transpose()).rowwise().squaredNorm().minCoeff(&closest_centroid);
  return closest_centroid;
}

void write_matrix_to_json(rapidjson::Writer<rapidjson::StringBuffer>* writer,
                          const Eigen::MatrixXf& matrix) {
  writer->StartArray();
  for (int i = 0; i < matrix.rows(); i++) {
    writer->StartArray();
    for (int j = 0; j < matrix.cols(); j++) {
      writer->Double(matrix(i, j));
    }
    writer->EndArray();
  }
  writer->EndArray();
}

void read_matrix_from_json(const rapidjson::Document::ValueType& doc, Eigen::MatrixXf* matrix) {
  DCHECK(doc.IsArray());
  for (const auto& [i, row] : Enumerate(doc.GetArray())) {
    DCHECK(row.IsArray());
    if (i == 0) {
      matrix->resize(doc.Size(), row.Size());
    }
    for (const auto& [j, val] : Enumerate(row.GetArray())) {
      matrix->operator()(i, j) = val.GetFloat();
    }
  }
}

std::string KMeans::ToJSON() {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartObject();
  writer.Key("k");
  writer.Int(k_);
  writer.Key("init_type");
  writer.Int(static_cast<int>(init_type_));
  writer.Key("centroids");
  write_matrix_to_json(&writer, centroids_);
  writer.EndObject();
  return sb.GetString();
}

void KMeans::FromJSON(std::string data) {
  rapidjson::Document doc;
  doc.Parse(data.data());
  DCHECK(doc.IsObject());
  DCHECK(doc.HasMember("k"));
  DCHECK(doc["k"].IsInt());
  DCHECK(doc.HasMember("init_type"));
  DCHECK(doc["init_type"].IsInt());
  DCHECK(doc.HasMember("centroids"));

  k_ = doc["k"].GetInt();
  init_type_ = KMeans::KMeansInitType(doc["init_type"].GetInt());
  read_matrix_from_json(doc["centroids"], &centroids_);
}

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
