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

#include "src/stirling/source_connectors/socket_tracer/protocols/http/credentials.h"

#include <openssl/sha.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

namespace px {
namespace stirling {
namespace protocols {
namespace http {

namespace {

constexpr size_t kFingerprintBytes = 8;
constexpr std::array<std::string_view, 2> kAuthorizationHeaders = {"authorization",
                                                                   "proxy-authorization"};
constexpr std::array<std::string_view, 3> kTokenHeaders = {"x-api-key", "x-auth-token", "api-key"};
constexpr std::array<std::string_view, 3> kUrlHeaders = {":path", "referer", "location"};
constexpr std::string_view kBearerSubprotocol = "base64url.bearer.authorization.k8s.io.";
constexpr std::array<std::string_view, 9> kCredentialFields = {
    "access_token", "id_token",      "refresh_token", "token",  "api_key",
    "apikey",       "client_secret", "password",      "secret"};
constexpr std::array<std::string_view, 11> kJwtClaims = {
    "iss", "sub", "aud", "azp", "client_id", "scope", "scp", "exp", "iat", "nbf", "kubernetes.io"};

template <size_t N>
bool Contains(const std::array<std::string_view, N>& names, std::string_view name) {
  return std::any_of(names.begin(), names.end(),
                     [name](std::string_view n) { return absl::EqualsIgnoreCase(n, name); });
}

std::string Marker(std::string_view secret, std::string_view details = "") {
  return absl::StrCat("<cred:sha256=", CredentialFingerprint(secret), details, ">");
}

bool DecodeJwtPart(std::string_view part, rapidjson::Document* doc) {
  std::string json;
  if (!absl::WebSafeBase64Unescape(part, &json)) {
    return false;
  }
  doc->Parse(json.data(), json.size());
  return !doc->HasParseError() && doc->IsObject();
}

std::optional<std::string> JwtClaims(std::string_view token) {
  std::vector<std::string_view> parts = absl::StrSplit(token, '.');
  rapidjson::Document header;
  rapidjson::Document payload;
  if (parts.size() != 3 || !DecodeJwtPart(parts[0], &header) ||
      !DecodeJwtPart(parts[1], &payload)) {
    return std::nullopt;
  }
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  writer.StartObject();
  auto alg = header.FindMember("alg");
  if (alg != header.MemberEnd()) {
    writer.Key("alg");
    alg->value.Accept(writer);
  }
  for (std::string_view claim : kJwtClaims) {
    auto it = payload.FindMember(rapidjson::StringRef(claim.data(), claim.size()));
    if (it != payload.MemberEnd()) {
      writer.Key(claim.data(), claim.size());
      it->value.Accept(writer);
    }
  }
  writer.EndObject();
  return std::string(buffer.GetString(), buffer.GetSize());
}

std::string RedactAuthorization(std::string_view value) {
  std::string_view trimmed = absl::StripAsciiWhitespace(value);
  const size_t space = trimmed.find(' ');
  if (space == std::string_view::npos) {
    return Marker(trimmed);
  }
  std::string_view scheme = trimmed.substr(0, space);
  std::string_view credential = absl::StripLeadingAsciiWhitespace(trimmed.substr(space + 1));
  std::string details;
  if (absl::EqualsIgnoreCase(scheme, "bearer")) {
    std::optional<std::string> claims = JwtClaims(credential);
    if (claims.has_value()) {
      details = absl::StrCat(" jwt=", claims.value());
    }
  } else if (absl::EqualsIgnoreCase(scheme, "basic")) {
    std::string decoded;
    if (absl::Base64Unescape(credential, &decoded)) {
      const size_t colon = decoded.find(':');
      if (colon != std::string::npos) {
        details = absl::StrCat(" user=", decoded.substr(0, colon));
      }
    }
  }
  return absl::StrCat(scheme, " ", Marker(credential, details));
}

std::string RedactSubprotocols(std::string_view value) {
  std::vector<std::string> protocols;
  for (std::string_view protocol : absl::StrSplit(value, ',')) {
    const size_t start = protocol.find(kBearerSubprotocol);
    if (start == std::string_view::npos) {
      protocols.emplace_back(protocol);
      continue;
    }
    std::string_view encoded = protocol.substr(start + kBearerSubprotocol.size());
    std::string token;
    std::optional<std::string> claims;
    if (absl::WebSafeBase64Unescape(encoded, &token)) {
      claims = JwtClaims(token);
    }
    protocols.push_back(absl::StrCat(
        protocol.substr(0, start + kBearerSubprotocol.size()),
        Marker(encoded, claims.has_value() ? absl::StrCat(" jwt=", claims.value()) : "")));
  }
  return absl::StrJoin(protocols, ",");
}

std::string RedactCookie(std::string_view cookie) {
  std::string_view pair = absl::StripLeadingAsciiWhitespace(cookie);
  const size_t eq = pair.find('=');
  if (eq == std::string_view::npos) {
    return std::string(pair);
  }
  return absl::StrCat(pair.substr(0, eq + 1), Marker(pair.substr(eq + 1)));
}

std::string RedactCookies(std::string_view value) {
  std::vector<std::string> cookies;
  for (std::string_view cookie : absl::StrSplit(value, ';')) {
    cookies.push_back(RedactCookie(cookie));
  }
  return absl::StrJoin(cookies, "; ");
}

std::string RedactSetCookie(std::string_view value) {
  const size_t attributes = std::min(value.find(';'), value.size());
  return absl::StrCat(RedactCookie(value.substr(0, attributes)), value.substr(attributes));
}

std::string RedactParams(std::string_view params) {
  std::vector<std::string> pairs;
  for (std::string_view pair : absl::StrSplit(params, '&')) {
    const size_t eq = pair.find('=');
    if (eq != std::string_view::npos && Contains(kCredentialFields, pair.substr(0, eq))) {
      pairs.push_back(absl::StrCat(pair.substr(0, eq + 1), Marker(pair.substr(eq + 1))));
    } else {
      pairs.emplace_back(pair);
    }
  }
  return absl::StrJoin(pairs, "&");
}

bool IsNameChar(char c) { return absl::ascii_isalnum(c) || c == '_' || c == '-' || c == '.'; }

size_t SkipSpaces(std::string_view text, size_t pos) {
  while (pos < text.size() && absl::ascii_isspace(text[pos])) {
    ++pos;
  }
  return pos;
}

std::string RedactCredentialFields(std::string_view text) {
  std::string out;
  size_t copied = 0;
  size_t i = 0;
  while (i < text.size()) {
    if (!IsNameChar(text[i]) || (i > 0 && IsNameChar(text[i - 1]))) {
      ++i;
      continue;
    }
    size_t end = i;
    while (end < text.size() && IsNameChar(text[end])) {
      ++end;
    }
    if (!Contains(kCredentialFields, text.substr(i, end - i))) {
      i = end;
      continue;
    }
    size_t pos = end < text.size() && text[end] == '"' ? end + 1 : end;
    pos = SkipSpaces(text, pos);
    if (pos >= text.size() || text[pos] != ':') {
      i = end;
      continue;
    }
    pos = SkipSpaces(text, pos + 1);
    if (pos >= text.size() || text[pos] != '"') {
      i = end;
      continue;
    }
    const size_t begin = pos + 1;
    size_t close = begin;
    while (close < text.size() && text[close] != '"') {
      close += text[close] == '\\' ? 2 : 1;
    }
    close = std::min(close, text.size());
    absl::StrAppend(&out, text.substr(copied, begin - copied),
                    Marker(text.substr(begin, close - begin)));
    copied = close;
    i = close;
  }
  absl::StrAppend(&out, text.substr(copied));
  return out;
}

}  // namespace

std::string CredentialFingerprint(std::string_view secret) {
  std::array<uint8_t, SHA256_DIGEST_LENGTH> digest;
  SHA256(reinterpret_cast<const uint8_t*>(secret.data()), secret.size(), digest.data());
  return absl::BytesToHexString(
      std::string_view(reinterpret_cast<const char*>(digest.data()), kFingerprintBytes));
}

std::optional<std::string> RedactCredentialHeader(std::string_view name, std::string_view value) {
  if (Contains(kAuthorizationHeaders, name)) {
    return RedactAuthorization(value);
  }
  if (Contains(kTokenHeaders, name)) {
    return Marker(value);
  }
  if (absl::EqualsIgnoreCase(name, "cookie")) {
    return RedactCookies(value);
  }
  if (absl::EqualsIgnoreCase(name, "set-cookie")) {
    return RedactSetCookie(value);
  }
  if (absl::EqualsIgnoreCase(name, "sec-websocket-protocol")) {
    return RedactSubprotocols(value);
  }
  if (Contains(kUrlHeaders, name)) {
    return RedactCredentialPath(value);
  }
  return std::nullopt;
}

std::string RedactCredentialPath(std::string_view path) {
  const size_t query = path.find('?');
  if (query == std::string_view::npos) {
    return std::string(path);
  }
  const size_t fragment = std::min(path.find('#', query), path.size());
  return absl::StrCat(path.substr(0, query + 1),
                      RedactParams(path.substr(query + 1, fragment - query - 1)),
                      path.substr(fragment));
}

std::string RedactCredentialBody(std::string_view body) {
  if (body.find('"') == std::string_view::npos) {
    return RedactParams(body);
  }
  return RedactCredentialFields(body);
}

}  // namespace http
}  // namespace protocols
}  // namespace stirling
}  // namespace px
