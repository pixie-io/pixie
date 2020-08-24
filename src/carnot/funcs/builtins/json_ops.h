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
  StringValue Exec(FunctionContext*, StringValue in, StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return "";
    }
    if (!d.HasMember(key.data())) {
      return "";
    }
    const auto& plucked_value = d[key.data()];
    if (plucked_value.IsNull()) {
      return "";
    }
    // This is robust to nested JSON.
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    plucked_value.Accept(writer);
    return sb.GetString();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Grabs the value for the key value the serialized JSON string and returns as a "
               "string.")
        .Details(
            "Convenience method to handle grabbing keys from a serialized JSON string. The "
            "function parses the JSON string and attempts to find the key. If the key is not "
            "found, an empty string is returned.\n"
            "This function returns the value as a string. If you want an int, use "
            "`px.pluck_int64`. If you want a float, use `px.pluck_float64`.")
        .Example(R"doc(
        | # df.quantiles holds "{'p50': 5.1, 'p90': 10}" on row 1.
        | df.p50 = px.pluck(df.quantiles, 'p50')
        | # df.p50 holds "5.1" as a string.
        )doc")
        .Arg("json_str", "JSON data serialized as a string.")
        .Arg("key", "The key to get the value for.")
        .Returns("The value for the key as a string.");
  }
};

class PluckAsInt64UDF : public udf::ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, StringValue in, StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return 0;
    }
    const auto& plucked_value = d[key.data()];
    return plucked_value.GetInt64();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Grabs the value for the key from the serialized JSON string and returns as an int.")
        .Details(
            "Convenience method to handle grabbing keys from a serialized JSON string. The "
            "function parses the JSON string and attempts to find the key. If the key is not "
            "found, 0 is returned. If the key is found, but the value cannot be parsed as an int, "
            "returns a 0.\n"
            "This function returns the value as an int. If you want a string, use `px.pluck`. If "
            "you want a float, use `px.pluck_float64`.")
        .Example(R"doc(
        | # df.http_data holds "{'status_code': 200, 'p50_latency': 5.1}" on row 1.
        | df.status_code = px.pluck_int64(df.http_data, 'status_code')
        | # df.status_code holds `200` as an int.
        )doc")
        .Arg("json_str", "JSON data serialized as a string.")
        .Arg("key", "The key to get the value for.")
        .Returns("The value for the key as an int.");
  }
};

class PluckAsFloat64UDF : public udf::ScalarUDF {
 public:
  Float64Value Exec(FunctionContext*, StringValue in, StringValue key) {
    rapidjson::Document d;
    rapidjson::ParseResult ok = d.Parse(in.data());
    // TODO(zasgar/michelle): Replace with null when available.
    if (ok == nullptr) {
      return 0.0;
    }
    const auto& plucked_value = d[key.data()];
    return plucked_value.GetDouble();
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Grabs the value for the key from the serialized JSON string and returns as a "
               "float.")
        .Details(
            "Convenience method to handle grabbing keys from a serialized JSON string. The "
            "function parses the JSON string and attempts to find the key. If the key is not "
            "found, 0.0 is returned. If the key is found, but the value cannot be parsed as an "
            "int, "
            "returns a 0.0.\n"
            "This function returns the value as a float. If you want a string, use `px.pluck`. "
            "If "
            "you want an int, use `px.pluck_int64`.")
        .Example(R"doc(
        | # df.http_data holds "{'status_code': 200, 'p50_latency': 5.1}" on row 1.
        | df.p50_latency = px.pluck_float64(df.http_data, 'p50_latency')
        | # df.p50_latency holds `5.1` as a float.
        )doc")
        .Arg("json_str", "JSON data serialized as a string.")
        .Arg("key", "The key to get the value for.")
        .Returns("The value for the key as a float");
  }
};

void RegisterJSONOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
