/*
 * Copyright © 2018- Pixie Labs Inc.
 * Copyright © 2020- New Relic, Inc.
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of New Relic Inc. and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Pixie Labs Inc. and its suppliers and
 * may be covered by U.S. and Foreign Patents, patents in process,
 * and are protected by trade secret or copyright law. Dissemination
 * of this information or reproduction of this material is strictly
 * forbidden unless prior written permission is obtained from
 * New Relic, Inc.
 *
 * SPDX-License-Identifier: Proprietary
 */

#pragma once

#include <string>

namespace px {
namespace stirling {
namespace java {
// Specification on Java symbol mangling:
// https://docs.oracle.com/en/java/javase/16/docs/specs/jni/types.html#type-signatures
std::string Demangle(const std::string& sym, std::string_view class_sig, std::string_view fn_sig);
}  // namespace java
}  // namespace stirling
}  // namespace px
