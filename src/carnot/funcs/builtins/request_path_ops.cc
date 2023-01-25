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

#include "src/carnot/funcs/builtins/request_path_ops.h"
#include <string_view>
#include <vector>
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace builtins {

void RegisterRequestPathOpsOrDie(udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<RequestPathClusteringPredictUDF>("_predict_request_path_cluster");
  registry->RegisterOrDie<RequestPathEndpointMatcherUDF>("_match_endpoint");
  /*****************************************
   * Aggregate UDFs.
   *****************************************/
  registry->RegisterOrDie<RequestPathClusteringFitUDA>("_build_request_path_clusters");
}

RequestPath::RequestPath(std::string request_path) {
  // Chop off request params for now. In the future, we want to keep these around and include
  // them in the clustering.
  std::string request_path_no_params;
  auto param_index = request_path.find('?');
  if (param_index == std::string::npos) {
    request_path_no_params = request_path;
  } else {
    request_path_no_params = request_path.substr(0, param_index);
  }
  if (request_path_no_params[0] == '/') {
    request_path_no_params = request_path_no_params.substr(1, request_path_no_params.size() - 1);
  }
  path_components_ = absl::StrSplit(request_path_no_params, '/');
}

double RequestPath::Similarity(const RequestPath& other) const {
  DCHECK_EQ(depth(), other.depth());
  auto num_agree = 0.0;
  for (const auto& [i, path_component] : Enumerate(path_components_)) {
    auto other_path_component = other.path_components_[i];
    if (path_component == kAnyToken || other_path_component == kAnyToken ||
        path_component != other_path_component) {
      continue;
    }
    num_agree += 1;
  }
  return num_agree / depth();
}

StatusOr<RequestPath> RequestPath::FromJSON(std::string serialized_request_path) {
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(serialized_request_path.data());
  if (ok == nullptr) {
    return error::InvalidArgument("RequestPath::FromJSON: cannot parse JSON");
  }
  return RequestPath::FromJSON(d);
}

StatusOr<RequestPath> RequestPath::FromJSON(const rapidjson::Document::ValueType& doc) {
  if (!doc.IsArray()) {
    return error::InvalidArgument(
        "RequestPath::FromJSON: expected array of path components, didn't receive array");
  }
  RequestPath request_path;
  for (rapidjson::Value::ConstValueIterator itr = doc.Begin(); itr != doc.End(); ++itr) {
    const rapidjson::Value& val = *itr;
    if (!val.IsString()) {
      return error::InvalidArgument(
          "RequestPath::FromJSON: expected array of path components, received non string path "
          "component");
    }
    request_path.path_components_.emplace_back(val.GetString(), val.GetStringLength());
  }
  return request_path;
}

std::string RequestPath::ToJSON() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  ToJSON(&writer);
  return sb.GetString();
}

void RequestPath::ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
  writer->StartArray();
  for (auto path : path_components_) {
    writer->String(path.data(), path.size());
  }
  writer->EndArray();
}

std::string RequestPath::ToString() const { return "/" + absl::StrJoin(path_components_, "/"); }

bool operator==(const RequestPath& a, const RequestPath& b) {
  if (a.depth() != b.depth()) {
    return false;
  }
  for (const auto& [i, path] : Enumerate(a.path_components())) {
    if (path != b.path_components()[i]) {
      return false;
    }
  }
  return true;
}

bool RequestPath::Matches(const RequestPath& templ) const {
  if (depth() != templ.depth()) {
    return false;
  }
  for (const auto& [i, path_component] : Enumerate(path_components_)) {
    if (templ.path_components()[i] == kAnyToken) {
      continue;
    }
    if (path_component != templ.path_components()[i]) {
      return false;
    }
  }
  return true;
}

void RequestPathCluster::Merge(const RequestPathCluster& other_cluster) {
  MergeCentroids(other_cluster.centroid_);
  MergeMembers(other_cluster.members_);
}
void RequestPathCluster::MergeCentroids(const RequestPath& other_centroid) {
  auto path_components = centroid_.path_components();
  auto other_path_components = other_centroid.path_components();
  for (const auto& [i, path] : Enumerate(other_path_components)) {
    if (path == path_components[i]) {
      continue;
    }
    centroid_.UpdatePathComponent(i, RequestPath::kAnyToken);
  }
}

void RequestPathCluster::MergeMembers(const absl::flat_hash_set<RequestPath>& other_members) {
  if (members_.size() > 0 && other_members.size() > 0) {
    for (auto path : other_members) {
      members_.insert(path);
    }
    if (members_.size() > min_cardinality_) {
      members_.clear();
    }
  } else {
    members_.clear();
  }
}

const RequestPath& RequestPathCluster::Predict(const RequestPath& request_path) const {
  // If path is in members
  if (members_.contains(request_path)) {
    return request_path;
  }
  return centroid_;
}

StatusOr<RequestPathCluster> RequestPathCluster::FromJSON(
    const rapidjson::Document::ValueType& doc) {
  if (!doc.IsObject()) {
    return error::InvalidArgument(
        "RequestPathCluster::FromJSON outer json must be object with centroid and member keys");
  }
  RequestPathCluster cluster;
  PX_ASSIGN_OR_RETURN(cluster.centroid_, RequestPath::FromJSON(doc[kCentroidKey]));
  const auto& members = doc[kMembersKey];
  if (!members.IsArray()) {
    return error::InvalidArgument("RequestPathCluster::FromJSON members key must be array");
  }
  for (rapidjson::Value::ConstValueIterator itr = members.Begin(); itr != members.End(); ++itr) {
    const rapidjson::Value& val = *itr;
    PX_ASSIGN_OR_RETURN(auto path, RequestPath::FromJSON(val));
    cluster.members_.insert(path);
  }
  return cluster;
}

void RequestPathCluster::ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
  writer->StartObject();
  writer->Key(kCentroidKey);
  centroid_.ToJSON(writer);
  writer->Key(kMembersKey);
  writer->StartArray();
  for (const auto& path : members_) {
    path.ToJSON(writer);
  }
  writer->EndArray();
  writer->EndObject();
}

double RequestPathClustering::MaxSimilarity(const RequestPath& request_path,
                                            int64_t* max_index) const {
  auto it = depth_to_centroid_indices_.find(request_path.depth());
  *max_index = -1;
  if (it == depth_to_centroid_indices_.end()) {
    return 0.0;
  }
  auto max_similarity = 0.0;
  for (auto index : it->second) {
    auto similarity = clusters_[index].Similarity(request_path);
    if (similarity > max_similarity) {
      *max_index = index;
      max_similarity = similarity;
    }
  }
  return max_similarity;
}

void RequestPathClustering::AddNewCluster(const RequestPathCluster& cluster) {
  auto centroid = cluster.centroid();
  if (depth_to_centroid_indices_.find(centroid.depth()) == depth_to_centroid_indices_.end()) {
    depth_to_centroid_indices_.emplace(centroid.depth(), std::vector<int64_t>());
  }
  depth_to_centroid_indices_[centroid.depth()].push_back(clusters_.size());
  clusters_.push_back(cluster);
}

void RequestPathClustering::MergeCluster(int64_t cluster_index,
                                         const RequestPathCluster& other_cluster) {
  clusters_[cluster_index].Merge(other_cluster);
}

StatusOr<RequestPathClustering> RequestPathClustering::FromJSON(const std::string& json) {
  rapidjson::Document d;
  rapidjson::ParseResult ok = d.Parse(json.data());
  if (ok == nullptr) {
    return error::InvalidArgument("RequestPathClustering::FromJSON: invalid json");
  }
  if (!d.IsArray()) {
    return error::InvalidArgument("RequestPathClustering::FromJSON: expected array");
  }

  RequestPathClustering clustering;
  for (rapidjson::Value::ConstValueIterator itr = d.Begin(); itr != d.End(); ++itr) {
    const rapidjson::Value& val = *itr;
    PX_ASSIGN_OR_RETURN(auto cluster, RequestPathCluster::FromJSON(val));
    clustering.AddNewCluster(cluster);
  }
  return clustering;
}

std::string RequestPathClustering::ToJSON() const {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartArray();
  for (auto cluster : clusters_) {
    cluster.ToJSON(&writer);
  }
  writer.EndArray();
  return sb.GetString();
}

const RequestPath& RequestPathClustering::Predict(const RequestPath& request_path) {
  int64_t closest_cluster_index;
  MaxSimilarity(request_path, &closest_cluster_index);
  if (closest_cluster_index == -1) {
    DCHECK(false) << absl::Substitute("Failed to find cluster close to request path $0",
                                      request_path.ToString());
    return request_path;
  }
  return clusters_[closest_cluster_index].Predict(request_path);
}

void RequestPathClustering::Update(const RequestPathCluster& new_cluster) {
  int64_t closest_cluster_index;
  auto similarity = MaxSimilarity(new_cluster.centroid(), &closest_cluster_index);
  if (closest_cluster_index == -1 || similarity < thresh_) {
    AddNewCluster(new_cluster);
  } else {
    MergeCluster(closest_cluster_index, new_cluster);
  }
}

void RequestPathClustering::Merge(const RequestPathClustering& other_clustering) {
  std::vector<RequestPathCluster> new_clusters;
  std::vector<RequestPathCluster> singleton_clusters;
  for (const auto& cluster : clusters_) {
    if (cluster.members().size() == 0) {
      new_clusters.push_back(cluster);
    } else {
      for (auto request_path : cluster.members()) {
        singleton_clusters.push_back(RequestPathCluster(request_path));
      }
    }
  }

  clusters_ = new_clusters;
  // Rebuild depth_to_cluster_indices mapping.
  depth_to_centroid_indices_.clear();
  for (const auto& [cluster_idx, cluster] : Enumerate(clusters_)) {
    auto depth = cluster.centroid().depth();
    if (depth_to_centroid_indices_.find(depth) == depth_to_centroid_indices_.end()) {
      depth_to_centroid_indices_.emplace(depth, std::vector<int64_t>());
    }
    depth_to_centroid_indices_[depth].push_back(cluster_idx);
  }

  for (const auto& cluster : other_clustering.clusters_) {
    if (cluster.members().size() == 0) {
      Update(cluster);
    } else {
      for (auto request_path : cluster.members()) {
        singleton_clusters.push_back(RequestPathCluster(request_path));
      }
    }
  }

  // Update clustering with the singleton points.
  for (const auto& cluster : singleton_clusters) {
    Update(cluster);
  }
}

}  // namespace builtins
}  // namespace carnot
}  // namespace px
