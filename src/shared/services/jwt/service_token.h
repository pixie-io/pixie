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

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include "src/common/base/base.h"

namespace px {
namespace services {

inline constexpr std::string_view kVizierAudience = "vizier";
inline constexpr std::string_view kPixieIssuer = "PL";
inline constexpr std::string_view kServiceScope = "service";

struct ServiceJWTOptions {
  // `aud` may be a string or an array of strings (RFC 7519 4.1.3); this value
  // need only appear among them.
  std::string audience{kVizierAudience};

  std::string issuer{kPixieIssuer};

  // Must all appear in the comma-joined `Scopes` claim. Authorization comes
  // from here, not from `sub`, which varies per caller.
  std::vector<std::string> required_scopes{std::string{kServiceScope}};

  // Applied to `exp` and `nbf`.
  std::chrono::seconds leeway{30};
};

// Returns OK only for a well-formed HS256 JWT with a valid signature, an `exp`
// that is present and unexpired, and claims satisfying `opts`.
//
// The Status message names the failing check, for operator logs. A caller
// serving untrusted peers should return something generic to the peer instead.
Status VerifyServiceJWT(std::string_view token, std::string_view signing_key,
                        const ServiceJWTOptions& opts = {});

// Mints a token VerifyServiceJWT accepts, with `service_id` recorded in the
// `ServiceID` claim.
StatusOr<std::string> GenerateServiceToken(std::string_view signing_key,
                                           std::string_view service_id);

// Returns the token following a case-insensitive "bearer " prefix, or an empty
// view if the prefix is absent. gRPC lowercases metadata keys but not values,
// and Pixie mints a lowercase prefix while RFC 6750 spells it "Bearer".
std::string_view StripBearerPrefix(std::string_view authorization_value);

}  // namespace services
}  // namespace px
