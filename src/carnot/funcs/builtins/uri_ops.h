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

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <uriparser/Uri.h>
#include <string>

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

class URIParseUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue uri) {
    UriUriA parsed;
    const char* errorPos;

    if (uriParseSingleUriA(&parsed, uri.c_str(), &errorPos) != URI_SUCCESS) {
      return "Failed to parse URI";
    }
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    writer.StartObject();
    WriteKeyVal(&writer, "scheme", &parsed.scheme);
    WriteKeyVal(&writer, "userInfo", &parsed.userInfo);
    WriteKeyVal(&writer, "host", &parsed.hostText);
    if (parsed.portText.first) {
      int port;
      if (absl::SimpleAtoi(absl::string_view(parsed.portText.first,
                                             parsed.portText.afterLast - parsed.portText.first),
                           &port)) {
        writer.Key("port");
        writer.Int(port);
      } else {
        uriFreeUriMembersA(&parsed);
        return "Port parsing failed.";
      }
    }
    if (parsed.pathHead) {
      const UriPathSegmentA* p = parsed.pathHead;
      std::string path;
      path.append(p->text.first, p->text.afterLast - p->text.first);
      while (p->next) {
        p = p->next;
        path.append("/");
        path.append(p->text.first, p->text.afterLast - p->text.first);
      }
      writer.Key("path");
      writer.String(path.c_str());
    }
    WriteKeyVal(&writer, "query", &parsed.query);
    WriteKeyVal(&writer, "fragment", &parsed.fragment);
    writer.EndObject();
    uriFreeUriMembersA(&parsed);
    return sb.GetString();
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Parses the URI into it's component parts and returns the parts "
               "as a JSON string.")
        .Example(R"doc(
        | df.uri = 'https://px.dev/community/?param1=val1'
        | df.parsed = px.uri_parse(df.uri)
        | df.path = px.pluck(df.parsed, 'path') # /community/
        )doc")
        .Arg("uri", "The uri to parse.")
        .Returns("A JSON string representation with the URI parts.");
  }

 private:
  void WriteKeyVal(rapidjson::Writer<rapidjson::StringBuffer>* writer, const char* key,
                   UriTextRangeA* range) {
    if (range->first) {
      writer->Key(key);
      writer->String(range->first, range->afterLast - range->first);
    }
  }
};

class URIRecomposeUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue scheme, StringValue userInfo, StringValue host,
                   Int64Value port, StringValue path, StringValue query, StringValue fragment) {
    if (port.val < 0) {
      return "Failed to recompose URI";
    }
    UriUriA parsed;

    UriHostDataA hostData;
    hostData.ip4 = nullptr;
    hostData.ip6 = nullptr;
    hostData.ipFuture.first = nullptr;
    parsed.hostData = hostData;

    UriPathSegmentA pathSegment;
    parsed.pathHead = nullptr;
    parsed.pathHead = nullptr;
    pathSegment.next = nullptr;
    if (path.size()) {
      SetURITextRange(&path, &pathSegment.text);
      parsed.pathHead = &pathSegment;
      parsed.pathHead = &pathSegment;
    }

    SetURITextRange(&scheme, &parsed.scheme);
    SetURITextRange(&userInfo, &parsed.userInfo);
    SetURITextRange(&host, &parsed.hostText);
    SetURITextRange(&query, &parsed.query);
    SetURITextRange(&fragment, &parsed.fragment);

    parsed.absolutePath = true;

    parsed.portText.first = nullptr;
    std::string portText;
    if (port.val) {
      portText = std::to_string(port.val);
      parsed.portText.first = portText.c_str();
      parsed.portText.afterLast = portText.c_str() + portText.size();
    }

    int charsRequired;
    if (uriToStringCharsRequiredA(&parsed, &charsRequired) != URI_SUCCESS) {
      return "Failed to recompose URI";
    }
    // The uriparse library writes a null char to the string at the end but doesn't include
    // that in the length computation.
    charsRequired++;

    std::string output;
    output.resize(charsRequired);
    if (uriToStringA(output.data(), &parsed, charsRequired, &charsRequired) != URI_SUCCESS) {
      return "Failed to recompose URI";
    }

    // Trim the null char written at the end.
    output.resize(charsRequired - 1);
    return output;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Recomposes the URI parts int a URI.")
        .Example(R"doc(
        | df.uri = px.uri_recompose('https', '', 'px.dev', 0, '/community/', 'param1=val1', '') # https://px.dev/community/?param1=val1
        )doc")
        .Arg("scheme", "The scheme for the URI.")
        .Arg("userInfo", "The userInfo for the URI.")
        .Arg("host", "The host for the URI.")
        .Arg("port", "The port for the URI.")
        .Arg("path", "The path for the URI.")
        .Arg("query", "The query for the URI.")
        .Arg("fragment", "The fragment for the URI.")
        .Returns("A fully composed URI.");
  }

 private:
  void SetURITextRange(StringValue* val, UriTextRangeA* range) {
    range->first = nullptr;
    if (val->size()) {
      range->first = val->c_str();
      range->afterLast = val->c_str() + val->size();
    }
  }
};

void RegisterURIOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
