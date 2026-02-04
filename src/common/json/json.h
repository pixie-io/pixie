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

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/types.h"
#include "src/common/base/utils.h"

namespace px {
namespace utils {

namespace internal {

inline rapidjson::Value JSONObj(const std::string& s,
                                rapidjson::Document::AllocatorType* /* allocator */) {
  rapidjson::Value json_obj;
  json_obj.SetString(rapidjson::StringRef(s.c_str(), s.length()));
  return json_obj;
}

inline rapidjson::Value JSONObj(std::string_view s,
                                rapidjson::Document::AllocatorType* /* allocator */) {
  rapidjson::Value json_obj;
  json_obj.SetString(rapidjson::StringRef(s.data(), s.length()));
  return json_obj;
}

inline rapidjson::Value JSONObj(int x, rapidjson::Document::AllocatorType* /* allocator */) {
  rapidjson::Value json_obj;
  json_obj.SetInt(x);
  return json_obj;
}

// TODO(oazizi): std::pair is treated as a key-value pair, but this might not be the caller's
// intention.
template <typename TKeyType, typename TValType>
inline rapidjson::Value JSONObj(const std::pair<TKeyType, TValType>& x,
                                rapidjson::Document::AllocatorType* allocator) {
  rapidjson::Value json_obj;
  json_obj.SetObject();
  json_obj.AddMember(JSONObj(x.first, allocator).Move(), JSONObj(x.second, allocator).Move(),
                     *allocator);
  return json_obj;
}

template <typename TValType>
rapidjson::Value JSONObj(const std::vector<TValType>& x,
                         rapidjson::Document::AllocatorType* allocator) {
  rapidjson::Value json_obj;
  json_obj.SetArray();
  for (const auto& v : x) {
    json_obj.PushBack(JSONObj(v, allocator).Move(), *allocator);
  }
  return json_obj;
}

template <typename TKeyType, typename TValType, typename TComparator, typename TAllocator>
rapidjson::Value JSONObj(const std::map<TKeyType, TValType, TComparator, TAllocator>& x,
                         rapidjson::Document::AllocatorType* allocator) {
  rapidjson::Value json_obj;
  json_obj.SetObject();
  for (const auto& [k, v] : x) {
    json_obj.AddMember(JSONObj(k, allocator).Move(), JSONObj(v, allocator).Move(), *allocator);
  }
  return json_obj;
}

template <typename TKeyType, typename TValType, typename TComparator, typename TAllocator>
rapidjson::Value JSONObj(const std::multimap<TKeyType, TValType, TComparator, TAllocator>& x,
                         rapidjson::Document::AllocatorType* allocator) {
  rapidjson::Value json_obj;
  json_obj.SetObject();
  for (const auto& [k, v] : x) {
    json_obj.AddMember(JSONObj(k, allocator).Move(), JSONObj(v, allocator).Move(), *allocator);
  }
  return json_obj;
}

}  // namespace internal

/**
 * Converts standard std types (e.g. std::string, std::vector, std::map) and their
 * compositions into a hierarchical JSON representation.
 *
 * Container-specific notes:
 *  - std::vector output will maintain its order.
 *  - std::map output will be sorted by key.
 *  - std::unordered_map (not yet implemented) will have an unspecified order.
 *
 * @tparam T type to convert into JSON. Should be automatically inferred.
 * @param x The object to convert into JSON representation.
 * @return JSON string.
 */
template <typename T>
std::string ToJSONString(const T& x) {
  rapidjson::Document d;
  rapidjson::Document::AllocatorType& allocator = d.GetAllocator();
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  rapidjson::Value obj = internal::JSONObj(x, &allocator);
  obj.Accept(writer);
  return sb.GetString();
}

/*
 * Exposes a limited set of APIs to build JSON string, with mixed data structures; which could not
 * be processed by the above ToJSONString().
 */
class JSONObjectBuilder {
 public:
  JSONObjectBuilder() : object_ended_{false}, buffer_{}, writer_{buffer_} { writer_.StartObject(); }

  // Closes JSON object and return the string.
  std::string GetString() {
    object_ended_ = true;
    writer_.EndObject();
    return buffer_.GetString();
  }

  // Writes a key-value pair.
  void WriteKV(std::string_view key, std::string_view value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.String(value.data(), value.size());
  }

  // Writes a key-value pair where value is an int.
  void WriteKV(std::string_view key, int value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.Int(value);
  }

  // Writes a key-value pair where value is an uint32_t.
  void WriteKV(std::string_view key, uint32_t value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.Uint(value);
  }

  // Writes a key-value pair where value is an int.
  void WriteKV(std::string_view key, int64_t value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.Int64(value);
  }

  // Writes a key-value pair where value is an uint64_t.
  void WriteKV(std::string_view key, uint64_t value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.Uint64(value);
  }

  // Writes a key-value pair where value is an array of strings.
  void WriteKV(std::string_view key, VectorView<std::string> value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.StartArray();
    for (auto v : value) {
      writer_.String(v.data(), v.size());
    }
    writer_.EndArray();
  }

  // Writes a key-value pair where value is an array of ints.
  void WriteKV(std::string_view key, VectorView<int32_t> value) {
    DCHECK(!object_ended_);
    writer_.String(key.data(), key.size());
    writer_.StartArray();
    for (auto v : value) {
      writer_.Int(v);
    }
    writer_.EndArray();
  }

  // Writes all values that are assigned to the keys sequentially.
  // The result is an array of objects, whose keys are specified by the input keys.
  // For example:
  // WriteRepeatedKVs("foo", {"a", "b"}, {"1", "2", "3", "4"});
  //
  // Returns: "foo": [{"a":"1","b":"2"}, {"a":"3","b":"4"}]
  void WriteRepeatedKVs(std::string_view key, const std::vector<std::string_view>& keys,
                        VectorView<std::string> values) {
    DCHECK(!object_ended_);
    DCHECK_EQ(values.size() % keys.size(), 0U);

    writer_.String(key.data(), key.size());
    writer_.StartArray();
    for (size_t i = 0; i < values.size(); ++i) {
      writer_.StartObject();
      WriteKV(keys[i % keys.size()], values[i]);
      writer_.EndObject();
    }
    writer_.EndArray();
  }

  // Writes a key and an object as value.
  // The ToJSON method of the object is called to recursively build a nested JSON structure.
  // For example:
  // WriteKVRecursive("partition", partition0);
  //
  // Returns: "partitions": {"name": "part0", ...}
  template <typename T>
  void WriteKVRecursive(std::string_view key, const T& value) {
    DCHECK(!object_ended_);

    writer_.String(key.data(), key.size());
    writer_.StartObject();
    value.ToJSON(this);
    writer_.EndObject();
  }

  // Writes a key and an array of objects as value.
  // For each object, the ToJSON method is called recursively to build a nested JSON structure.
  // For example:
  // WriteKVArrayRecursive("partitions", {partition0, partition1});
  //
  // Returns: "partitions": [{"name": "part0", ...}, {"name": "part1", ...}]
  template <typename T>
  void WriteKVArrayRecursive(std::string_view key, const VectorView<T>& values) {
    DCHECK(!object_ended_);

    writer_.String(key.data(), key.size());
    writer_.StartArray();
    for (size_t i = 0; i < values.size(); ++i) {
      writer_.StartObject();
      values[i].ToJSON(this);
      writer_.EndObject();
    }
    writer_.EndArray();
  }

 private:
  bool object_ended_;
  rapidjson::StringBuffer buffer_;
  rapidjson::Writer<rapidjson::StringBuffer> writer_;
};

}  // namespace utils
}  // namespace px
