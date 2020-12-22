#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/utils.h"

namespace pl {
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

}  // namespace utils
}  // namespace pl
