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

#include <absl/strings/strip.h>
#include <algorithm>
#include <string>
#include "src/carnot/udf/registry.h"
#include "src/common/base/utils.h"
#include "src/shared/types/types.h"

namespace px {
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
        | df.length = px.length(df.service) # 8
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
        .Example(R"doc(df.svc_name = "pixie-labs"
        | df.found = px.find(df.svc_name, '-labs') # 5)doc")
        .Arg("arg1", "The string to search through.")
        .Arg("arg2", "The substring to find.")
        .Returns("The index of the first occurence of the substring. -1 if no match is found.");
  }
};

class SubstringUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1, Int64Value pos, Int64Value length) {
    // If the pos is "erroneous" then just return empty string.
    if (pos < 0 || pos > static_cast<int64_t>(b1.length()) || length < 0) {
      return "";
    }
    return b1.substr(static_cast<size_t>(pos.val), static_cast<size_t>(length.val));
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Returns the specified substring from the string")
        .Details(
            "Extracts the substring from the string starting at index `pos` and for `length` "
            "characters. If `pos > len(string)`, `px.substr` returns the "
            "empty string. If `pos < len(string)` but `pos + length > len(string)`, `px.substr` "
            "returns the maximum length substring starting at `pos`")
        .Example(R"doc(df.service = 'checkout'
        | df.str = px.substring(df.service, 1, 5) # 'hecko'
        )doc")
        .Arg("string", "The string to get the substring from.")
        .Arg("pos", "The position to start the substring, inclusive.")
        .Arg("length", "The length of the substring to return.")
        .Returns("The substring from `string`.");
  }
};

class ToLowerUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1) {
    transform(b1.begin(), b1.end(), b1.begin(), ::tolower);
    return b1;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Transforms all uppercase ascii characters in the string to lowercase.")
        .Example(R"doc(df.service  = "Kelvin"
        | df.lower = px.tolower(df.service) # "kelvin"
        )doc")
        .Arg("string", "The string to transform.")
        .Returns("`string` with all uppercase ascii converted to lowercase.");
  }
};

class ToUpperUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue b1) {
    transform(b1.begin(), b1.end(), b1.begin(), ::toupper);
    return b1;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Transforms all lowercase ascii characters in the string to uppercase.")
        .Example(R"doc(df.service = Kelvin
        | df.upper = px.toupper(df.service) # "KELVIN"
        )doc")
        .Arg("string", "The string to transform.")
        .Returns("`string` with all lowercase ascii converted to uppercase.");
  }
};

class TrimUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue s) {
    std::string val = s;
    absl::StripAsciiWhitespace(&val);
    return val;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Trim ascii whitespace from before and after the string content.")
        .Details(
            "Returns a copy of the string with the white space before and after the string trimmed "
            "away. Does not affect whitespace in between words.")
        .Example(R"doc(df.service = "        pl/kelvin "
        | df.trimmed = px.trim(df.service) # "pl/kelvin"
        )doc")
        .Arg("string", "The string to transform.")
        .Returns("The string but with leading and trailing whitespace removed.");
  }
};

class StripPrefixUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue prefix, StringValue s) {
    return StringValue(absl::StripPrefix(s, prefix));
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Strips the specified prefix from the string.")
        .Details(
            "Returns the string with the prefix removed. Will return the same string if the prefix "
            "not found.")
        .Example(R"doc(# df.service is `pl/kelvin`
        | df.removed_pl = px.strip_prefix('pl/', df.service) # "kelvin"
        )doc")
        .Arg("prefix", "The prefix to remove.")
        .Arg("string", "The string value to strip the prefix from.")
        .Returns("`string` with `prefix` removed from the beginning if it existed.");
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

class BytesToHex : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue h) { return BytesToString<bytes_format::Hex>(h); }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert an input bytes in hex string.")
        .Details("This function converts an input bytes sequence in hex string.")
        .Example("df.hex = px.bytes_to_hex(df.resp)")
        .Arg("arg1", "The bytes sequence.")
        .Returns("The input converted to a hex string.");
  }
};

class StringToIntUDF : public udf::ScalarUDF {
 public:
  Int64Value Exec(FunctionContext*, StringValue input, Int64Value default_val) {
    int64_t val;
    if (!absl::SimpleAtoi(input, &val)) {
      return default_val;
    }
    return val;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a string to an integer.")
        .Details(
            "This function parses a string into a 64-bit integer if possible, otherwise it returns "
            "the default value.")
        .Example("df.val = px.atoi(df.int_as_str_col, -1)")
        .Arg("arg1", "The string to convert")
        .Arg("arg2", "The default integer value to return if conversion fails")
        .Returns(
            "Integer version of string, if parsing succeeded otherwise the default value passed "
            "in.");
  }
};

class IntToStringUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value input) { return std::to_string(input.val); }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert an integer into a string.")
        .Details("This function converts an integer into a string.")
        .Example("df.val = px.itoa(df.int_col)")
        .Arg("arg1", "The integer to convert.")
        .Returns("The input converted to a string.");
  }
};

void RegisterStringOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
