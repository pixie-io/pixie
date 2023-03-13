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

// This file defines some commonly used macros in our code base.

#pragma once

// Warn if a result is unused.
#ifdef __clang__
#define PX_MUST_USE_RESULT __attribute__((warn_unused_result))
#else
#define PX_MUST_USE_RESULT
#endif

#define PX_UNUSED(x) (void)(x)

// Internal helper for concatenating macro values.
#define PX_CONCAT_NAME_INNER(x, y) x##y
#define PX_CONCAT_NAME(x, y) PX_CONCAT_NAME_INNER(x, y)
#define PX_UNIQUE_NAME(name) PX_CONCAT_NAME(name, __COUNTER__)

// Disable clang format since it insists on inlining these macros.
// clang-format off
#if defined(__clang__)

#define PX_SUPPRESS_WARNINGS_START()                          \
  _Pragma("clang diagnostic push")                            \
  _Pragma("clang diagnostic ignored \"-Weverything\"")

#define PX_SUPPRESS_WARNINGS_END()                            \
  _Pragma("clang diagnostic pop")

#elif defined(__GNUC__) || defined(__GNUG__)

// GCC does not support disabling all warnings, so we just suppress the most common one.
// We can add to this list if needed.
#define PX_SUPPRESS_WARNINGS_START()                         \
  _Pragma("GCC diagnostic push")                             \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"")    \
  _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")   \
  _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")


#define PX_SUPPRESS_WARNINGS_END()                           \
  _Pragma("GCC diagnostic pop")

#else
#error "Unsupported compiler"
#endif

// Branch predictor macros to use mostly in debug code to disable it.
#if defined(__GNUC__)
#define PX_LIKELY(x) (__builtin_expect((x), 1))
#define PX_UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define PX_LIKELY(x) (x)
#define PX_UNLIKELY(x) (x)
#endif

// For debugging.
#define PX_LOG_VAR(var) LOG(INFO) << #var ": " << var;

// clang-format on
