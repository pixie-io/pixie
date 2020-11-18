#pragma once

#include <string>
#include <utility>
#include <vector>

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
        | df.quantiles = '{"p50": 5.1, "p90": 10}'
        | df.p50 = px.pluck(df.quantiles, 'p50') # "5.1", as a string.
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
        | df.http_data = '{"status_code": 200, "p50_latency": 5.1}'
        | df.status_code = px.pluck_int64(df.http_data, 'status_code') # 200
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
        | df.http_data = '{"status_code": 200, "p50_latency": 5.1}'
        | df.p50_latency = px.pluck_float64(df.http_data, 'p50_latency') # 5.1
        )doc")
        .Arg("json_str", "JSON data serialized as a string.")
        .Arg("key", "The key to get the value for.")
        .Returns("The value for the key as a float");
  }
};

/**
  DocString intentionally omitted, this is a non-public function.
  This function creates a custom deep link by creating a "script reference" from a label,
  script name, and input script arguments. The compiler translates the public API into this UDF,
  and the public API will be documented in the compile time functions.

  ScriptReferenceUDF takes in a label, script, and set of variadic script args.
  These script args passed in the alternating form argname0, argval0, argname1., argval1.
  Since script args are always expressed as strings in vis specs, these arg values are
  also passed in as strings. (When a script is executed, its script args are parsed by
  the compiler into their proper data type).
  TODO(nserrino, philkuz): Update this logic when we add support for object types.
 */
template <typename... T>
class ScriptReferenceUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue label, StringValue script, T... args) {
    return ExecImpl(label, script, {std::forward<T>(args)...});
  }

  // Some hacky stuff due to variadic args....
  static udf::InfRuleVec SemanticInferenceRules() {
    const std::size_t num_script_args = sizeof...(T);
    auto num_input_args = num_script_args + 2;  // add 1 each for script and label.
    std::vector<types::SemanticType> input_st(num_input_args, types::SemanticType::ST_NONE);

    return {udf::ExplicitRule::Create<ScriptReferenceUDF<T...>>(
        types::SemanticType::ST_SCRIPT_REFERENCE, input_st)};
  }

 private:
  StringValue ExecImpl(StringValue label, StringValue script,
                       std::initializer_list<StringValue> values) {
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("label", rapidjson::Value().SetString(label.c_str(), d.GetAllocator()).Move(),
                d.GetAllocator());
    d.AddMember("script", rapidjson::Value().SetString(script.c_str(), d.GetAllocator()).Move(),
                d.GetAllocator());

    // Construct the args object
    rapidjson::Value argsObj;
    argsObj.SetObject();

    std::string arg_name;
    int32_t counter = 0;
    for (const auto& arg_val : values) {
      if (counter % 2) {
        argsObj.AddMember(rapidjson::Value().SetString(arg_name.c_str(), d.GetAllocator()).Move(),
                          rapidjson::Value().SetString(arg_val.c_str(), d.GetAllocator()).Move(),
                          d.GetAllocator());
      } else {
        arg_name = arg_val;
      }
      counter++;
    }
    d.AddMember("args", argsObj.Move(), d.GetAllocator());

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);
    return sb.GetString();
  }
};

void RegisterJSONOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
