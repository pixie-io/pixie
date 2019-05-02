#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace builtins {

// TODO(zasgar): PL-419 To have proper support for JSON we need structs and nullable types.
// Revisit when we have them.
class PluckUDF : public udf::ScalarUDF {
 public:
  types::StringValue Exec(udf::FunctionContext *, types::StringValue in, types::StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return "";
    }
    if (!d.HasMember(key.data())) {
      return "";
    }
    const auto &plucked_value = d[key.data()];
    if (plucked_value.IsNull()) {
      return "";
    }
    // This is robust to nested JSON.
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    plucked_value.Accept(writer);
    return sb.GetString();
  }
};

class PluckAsInt64UDF : public udf::ScalarUDF {
 public:
  types::Int64Value Exec(udf::FunctionContext *, types::StringValue in, types::StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return 0;
    }
    const auto &plucked_value = d[key.data()];
    return plucked_value.GetInt64();
  }
};

class PluckAsFloat64UDF : public udf::ScalarUDF {
 public:
  types::Float64Value Exec(udf::FunctionContext *, types::StringValue in, types::StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return 0.0;
    }
    const auto &plucked_value = d[key.data()];
    return plucked_value.GetDouble();
  }
};

inline void RegisterJSONOpsOrDie(udf::ScalarUDFRegistry *registry) {
  registry->RegisterOrDie<PluckUDF>("pl.pluck");
  registry->RegisterOrDie<PluckAsInt64UDF>("pl.pluck_int64");
  registry->RegisterOrDie<PluckAsFloat64UDF>("pl.pluck_float64");
}

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
