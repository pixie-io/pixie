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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <absl/strings/substitute.h>
#include "third_party/eigen3/Eigen/Core"

#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace exec {
namespace ml {

class WeightedPointSet {
 public:
  WeightedPointSet() : size_(0) {}
  WeightedPointSet(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights) {
    DCHECK_EQ(points.rows(), weights.rows());
    size_ = points.rows();
    point_size_ = points.cols();
    points_ = points;
    weights_ = weights;
  }
  WeightedPointSet(int size, int point_size) : size_(size), point_size_(point_size) {
    points_.resize(size, point_size);
    weights_.resize(size);
  }

  static std::shared_ptr<WeightedPointSet> Union(
      const std::vector<std::shared_ptr<WeightedPointSet>>& point_sets) {
    DCHECK_GT(point_sets.size(), 0U);
    int union_size = 0;
    for (auto set : point_sets) {
      union_size += set->size();
    }
    auto union_set = std::make_shared<WeightedPointSet>(union_size, point_sets[0]->point_size());
    auto index = 0;
    for (auto set : point_sets) {
      auto end_of_set = index + set->size() - 1;
      (*union_set).points_(Eigen::seq(index, end_of_set), Eigen::indexing::all) = set->points();
      (*union_set).weights_(Eigen::seq(index, end_of_set)) = set->weights();
      index = end_of_set + 1;
    }
    union_set->size_ = union_size;
    return union_set;
  }

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) {
    writer->StartObject();
    writer->Key("points");
    writer->StartArray();
    for (int i = 0; i < size_; i++) {
      writer->StartArray();
      for (int j = 0; j < point_size_; j++) {
        writer->Double(points_(i, j));
      }
      writer->EndArray();
    }
    writer->EndArray();
    writer->Key("weights");
    writer->StartArray();
    for (int i = 0; i < size_; i++) {
      writer->Double(weights_(i));
    }
    writer->EndArray();
    writer->EndObject();
  }

  void FromJSON(const rapidjson::Document::ValueType& doc) {
    DCHECK(doc.IsObject());
    DCHECK(doc.HasMember("points"));
    DCHECK(doc["points"].IsArray());
    DCHECK(doc.HasMember("weights"));
    DCHECK(doc["weights"].IsArray());

    const rapidjson::Value& points = doc["points"];
    const rapidjson::Value& weights = doc["weights"];
    DCHECK_EQ(points.Size(), weights.Size());
    size_ = points.Size();
    weights_.resize(size_);
    for (rapidjson::SizeType i = 0; i < points.Size(); i++) {
      const rapidjson::Value& point_json = points[i];
      if (i == 0) {
        point_size_ = point_json.Size();
        points_.resize(size_, point_size_);
      }
      for (rapidjson::SizeType j = 0; j < point_json.Size(); j++) {
        points_(i, j) = point_json[j].GetFloat();
      }
      weights_(i) = weights[i].GetFloat();
    }
    size_ = points.Size();
  }

  static std::shared_ptr<WeightedPointSet> CreateFromJSON(
      const rapidjson::Document::ValueType& doc) {
    auto set = std::make_shared<WeightedPointSet>();
    set->FromJSON(doc);
    return set;
  }

  const Eigen::MatrixXf& points() const { return points_; }
  const Eigen::VectorXf& weights() const { return weights_; }
  int point_size() const { return point_size_; }
  int size() const { return size_; }

 protected:
  int size_;
  int point_size_;
  Eigen::MatrixXf points_;
  Eigen::VectorXf weights_;
};

class KMeansCoreset : public WeightedPointSet {
 public:
  KMeansCoreset(int coreset_size, int d) : WeightedPointSet(coreset_size, d) {}
  static std::shared_ptr<KMeansCoreset> FromWeightedPointSet(std::shared_ptr<WeightedPointSet> set,
                                                             size_t coreset_size);

 private:
  /**
   * Constructs a kmeans coreset from a set of points, using the algorithm in
   * https://arxiv.org/abs/1702.08248.
   * Modifies the algorithm slightly to allow for weighted point set input rather than just point
   * set input.
   **/
  void Construct(const Eigen::MatrixXf& points, const Eigen::VectorXf& weights);
};

template <typename TCoreset>
class CoresetTree {
  using Level = std::vector<std::shared_ptr<WeightedPointSet>>;

 public:
  /**
   * r-way Coreset Tree.
   **/
  CoresetTree(size_t r, size_t coreset_size) : coreset_size_(coreset_size), r_(r) {}

  void Update(std::shared_ptr<WeightedPointSet> set) {
    if (levels_.size() == 0) {
      levels_.emplace_back();
    }
    levels_[0].push_back(set);
    auto i = 0UL;
    while (levels_[i].size() >= r_) {
      auto merged =
          TCoreset::FromWeightedPointSet(WeightedPointSet::Union(levels_[i]), coreset_size_);
      levels_[i].clear();
      if (levels_.size() <= i + 1) {
        levels_.emplace_back();
      }
      levels_[i + 1].push_back(std::move(merged));
      i++;
    }
  }

  std::shared_ptr<WeightedPointSet> Coreset() {
    std::vector<std::shared_ptr<WeightedPointSet>> flat_levels;
    for (const auto& [i, level] : Enumerate(levels_)) {
      if (level.size() > 0) {
        flat_levels.insert(flat_levels.end(), level.begin(), level.end());
      }
    }
    if (flat_levels.size() == 0) {
      return std::make_shared<WeightedPointSet>();
    }
    return WeightedPointSet::Union(flat_levels);
  }

  void Merge(const CoresetTree<TCoreset>& other) {
    for (auto i = 0UL; i < levels_.size(); i++) {
      if (i < other.levels_.size() && other.levels_[i].size() > 0) {
        levels_[i].insert(levels_[i].end(), other.levels_[i].begin(), other.levels_[i].end());
      }
    }
    for (auto i = levels_.size(); i < other.levels_.size(); i++) {
      if (other.levels_[i].size() > 0) {
        levels_.emplace_back();
        levels_[i].insert(levels_[i].end(), other.levels_[i].begin(), other.levels_[i].end());
      }
    }

    // Fix the r-way tree by coresetting any levels that have r or more buckets after merge.
    for (auto i = 0UL; i < levels_.size(); i++) {
      if (levels_[i].size() >= r_) {
        auto merged =
            TCoreset::FromWeightedPointSet(WeightedPointSet::Union(levels_[i]), coreset_size_);
        levels_[i].clear();
        if (i == levels_.size() - 1) {
          levels_.emplace_back();
        }
        levels_[i + 1].push_back(std::move(merged));
      }
    }
  }

  void ToJSON(rapidjson::Writer<rapidjson::StringBuffer>* writer) const {
    writer->StartObject();
    writer->Key("coreset_size");
    writer->Uint(coreset_size_);
    writer->Key("r");
    writer->Uint(r_);
    writer->Key("levels");
    writer->StartObject();
    for (auto i = 0UL; i < levels_.size(); i++) {
      if (levels_[i].size() > 0) {
        writer->Key(absl::Substitute("$0", i).data());
        writer->StartArray();
        for (auto set : levels_[i]) {
          set->ToJSON(writer);
        }
        writer->EndArray();
      }
    }
    writer->EndObject();
    writer->EndObject();
  }

  void FromJSON(const rapidjson::Document::ValueType& doc) {
    DCHECK(doc.IsObject());
    DCHECK(doc.HasMember("coreset_size"));
    DCHECK(doc["coreset_size"].IsUint());
    DCHECK(doc.HasMember("r"));
    DCHECK(doc["r"].IsUint());
    DCHECK(doc.HasMember("levels"));
    DCHECK(doc["levels"].IsObject());
    coreset_size_ = doc["coreset_size"].GetUint();
    r_ = doc["r"].GetUint();
    levels_.clear();

    for (rapidjson::Value::ConstMemberIterator itr = doc["levels"].MemberBegin();
         itr != doc["levels"].MemberEnd(); ++itr) {
      size_t level_ind;
      if (!absl::SimpleAtoi(itr->name.GetString(), &level_ind)) {
        // Skip any keys that aren't numbers.
        continue;
      }
      Level level;
      for (rapidjson::SizeType i = 0; i < itr->value.Size(); i++) {
        level.push_back(WeightedPointSet::CreateFromJSON(itr->value[i]));
      }
      if (level_ind >= levels_.size()) {
        levels_.resize(level_ind + 1);
      }
      levels_[level_ind] = level;
    }
  }

 private:
  size_t coreset_size_;
  size_t r_;
  std::vector<Level> levels_;
};

template <typename TCoresetStructure>
class CoresetDriver {
 public:
  template <typename... Args>
  CoresetDriver(int m, int d, Args... args)
      : m_(m), d_(d), coreset_data_(args...), points_(m_, d_), weights_(m_), size_(0) {}

  void Update(const Eigen::VectorXf& p) {
    points_(size_, Eigen::indexing::all) = p.transpose();
    weights_(size_) = 1.0f;
    size_++;
    if (size_ == m_) {
      coreset_data_.Update(std::make_shared<WeightedPointSet>(points_, weights_));
      size_ = 0;
    }
  }

  std::shared_ptr<WeightedPointSet> Query() {
    auto coreset = coreset_data_.Coreset();
    if (size_ == 0) {
      // If we haven't seen any data, this could return an empty set.
      return coreset;
    }
    if (coreset->size() == 0) {
      return CurrentSet();
    }
    return WeightedPointSet::Union({coreset, CurrentSet()});
  }

  void Merge(const CoresetDriver<TCoresetStructure>& other) {
    coreset_data_.Merge(other.coreset_data_);
    auto new_set = WeightedPointSet::Union({CurrentSet(), other.CurrentSet()});
    if (new_set->size() >= m_) {
      coreset_data_.Update(new_set);
      size_ = 0;
    } else {
      GatherPointsFromSet(new_set);
    }
  }

  std::string ToJSON() const {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    writer.Key("base_set");
    CurrentSet()->ToJSON(&writer);
    writer.Key("coreset");
    coreset_data_.ToJSON(&writer);
    writer.EndObject();
    return sb.GetString();
  }

  void FromJSON(std::string data) {
    rapidjson::Document doc;
    doc.Parse(data.data());
    DCHECK(doc.IsObject());
    DCHECK(doc.HasMember("base_set"));
    DCHECK(doc.HasMember("coreset"));
    auto set = WeightedPointSet::CreateFromJSON(doc["base_set"]);
    GatherPointsFromSet(set);
    coreset_data_.FromJSON(doc["coreset"]);
  }

 private:
  std::shared_ptr<WeightedPointSet> CurrentSet() const {
    if (size_ == 0) {
      return std::make_shared<WeightedPointSet>(0, points_.cols());
    }
    return std::make_shared<WeightedPointSet>(
        points_(Eigen::seq(0, size_ - 1), Eigen::indexing::all),
        weights_(Eigen::seq(0, size_ - 1)));
  }
  void GatherPointsFromSet(std::shared_ptr<WeightedPointSet> set) {
    size_ = set->size();
    if (set->size() == 1) {
      points_(0, Eigen::indexing::all) = set->points()(0, Eigen::indexing::all);
      weights_(0) = set->weights()(0);
    } else if (set->size() > 1) {
      points_(Eigen::seq(0, set->size() - 1), Eigen::indexing::all) = set->points();
      weights_(Eigen::seq(0, set->size() - 1)) = set->weights();
    }
  }
  int m_;
  int d_;
  TCoresetStructure coreset_data_;
  Eigen::MatrixXf points_;
  Eigen::VectorXf weights_;
  int size_;
};

}  // namespace ml
}  // namespace exec
}  // namespace carnot
}  // namespace px
