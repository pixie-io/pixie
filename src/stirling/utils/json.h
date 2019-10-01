#pragma once

#include <rapidjson/writer.h>
#include <map>
#include <string>

namespace pl {
namespace stirling {
namespace utils {

template <typename TStringMapType>
inline std::string WriteMapAsJSON(const TStringMapType& map) {
  rapidjson::StringBuffer sb;
  rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
  writer.StartObject();
  for (const auto& [k, v] : map) {
    writer.String(k.data(), k.size());
    writer.String(v.data(), v.size());
  }
  writer.EndObject();
  return sb.GetString();
}

}  // namespace utils
}  // namespace stirling
}  // namespace pl
