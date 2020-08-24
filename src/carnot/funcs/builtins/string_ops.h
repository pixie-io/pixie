#pragma once

#include <algorithm>
#include <string>
#include "src/carnot/udf/registry.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace builtins {

class ContainsUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, StringValue b1, StringValue b2) {
    return absl::StrContains(b1, b2);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Returns whether the first string contains the second string.")
        .Example("matching_df = matching_df[px.contains(matching_df.svc_names, 'my_svc')]")
        .Arg("arg1", "The string that should contain the second string.")
        .Arg("arg2", "The string that should be contained in the first string.")
        .Returns("A boolean of whether the first string contains the second string.");
  }
};

class LengthUDF : public udf::ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, StringValue b1) { return b1.length(); }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Returns the length of the string")
        .Example(R"doc(df.service = 'checkout'
        | df.length = px.length(df.service)
        )doc")
        .Arg("s", "The string to get the length of")
        .Returns("The length of the string.");
  }
};

class FindUDF : public udf::ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, StringValue src, StringValue substr) {
    return src.find(substr);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Find the index of the first occurrence of the substring.")
        .Details(
            "Returns the index of the first occurrence of the substring in the given string. If no "
            "match is found, returns -1.")
        .Example("df.found = px.Find(df.svc_name, 'pixie-')")
        .Arg("arg1", "The string to search through.")
        .Arg("arg2", "The substring to find.")
        .Returns("The index of the first occurence of the substring. -1 if no match is found.");
  }
};

class SubstringUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1, Int64Value pos, Int64Value length) {
    return b1.substr(static_cast<size_t>(pos.val), static_cast<size_t>(length.val));
  }
};

class ToLowerUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1) {
    transform(b1.begin(), b1.end(), b1.begin(), ::tolower);
    return b1;
  }
};

class ToUpperUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1) {
    transform(b1.begin(), b1.end(), b1.begin(), ::toupper);
    return b1;
  }
};

class TrimUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c) { return std::isspace(c); });
    auto wsback =
        std::find_if_not(s.rbegin(), s.rend(), [](int c) { return std::isspace(c); }).base();
    return (wsback <= wsfront ? StringValue(std::string())
                              : StringValue(std::string(wsfront, wsback)));
  }
};

class StripPrefixUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue prefix, StringValue s) {
    return StringValue(absl::StripPrefix(s, prefix));
  }
};

class HexToASCII : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue h) {
    std::string result;
    auto s_or_res = AsciiHexToBytes<std::string>(h);
    if (s_or_res.ok()) {
      return s_or_res.ConsumeValueOrDie();
    }
    return "";
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert an input hex sequence in ASCII to bytes.")
        .Details(
            "This function converts an input hex sequence in ASCII to bytes. "
            "The input must be a well-formed hex representation, with optional separator."
            "If the input is invalid, it will return an empty string.")
        .Example("df.asciiBytes = px.hex_to_ascii(df.resp)")
        .Arg("arg1", "The ascii hex sequence to convert to bytes.")
        .Returns("The input converted to a sequence of bytes, or empty string if invalid.");
  }
};

void RegisterStringOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
