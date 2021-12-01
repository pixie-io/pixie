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

#include <absl/strings/str_cat.h>
#include <iostream>
#include <string_view>

#include "src/common/base/logging.h"
#include "src/stirling/source_connectors/perf_profiler/java/demangle.h"

namespace {

std::string DemangleClassName(std::string_view* mangled) {
  // DemangleClassName() needs to consume from its input,
  // so the input string_view 'mangled' is passed in by pointer.
  std::string class_name;
  while (!mangled->empty()) {
    const char c = mangled->front();
    mangled->remove_prefix(1);
    if (c == ';') {
      break;
    } else if (c == '/') {
      class_name += ".";
    } else {
      class_name += c;
    }
  }
  return class_name;
}

std::string DemangleKernel(std::string_view mangled) {
  // Specification on Java symbol mangling:
  // https://docs.oracle.com/en/java/javase/16/docs/specs/jni/types.html#type-signatures
  std::string accumulator;
  uint64_t narr = 0;
  uint64_t ndemangled = 0;

  auto push_type = [&](const auto& type_string) {
    if (ndemangled > 0) {
      accumulator += ", ";
    }
    ++ndemangled;
    accumulator += type_string;
    while (narr > 0) {
      accumulator += "[]";
      --narr;
    }
  };

  while (!mangled.empty()) {
    const char c = mangled.front();
    mangled.remove_prefix(1);
    DCHECK_NE(c, '(');
    DCHECK_NE(c, ')');
    if (c == '[') {
      ++narr;
    } else if (c == 'L') {
      // We need the function DemangleClassName() to consume characters from 'mangled',
      // so, 'mangled' is passed in by pointer.
      const std::string class_name = DemangleClassName(&mangled);
      push_type(class_name);
    } else if (c == 'Z') {
      push_type("boolean");
    } else if (c == 'B') {
      push_type("byte");
    } else if (c == 'C') {
      push_type("char");
    } else if (c == 'S') {
      push_type("short");
    } else if (c == 'I') {
      push_type("int");
    } else if (c == 'J') {
      push_type("long");
    } else if (c == 'F') {
      push_type("float");
    } else if (c == 'D') {
      push_type("double");
    } else if (c == 'V') {
      push_type("void");
    }
  }
  return accumulator;
}

}  // namespace

namespace px {
namespace stirling {
namespace java {

std::string Demangle(const std::string& sym, std::string_view class_sig, std::string_view fn_sig) {
  if (fn_sig.empty() && class_sig.empty()) {
    return sym;
  }

  std::string demangled_args;
  std::string demangled_type;
  std::string demangled_class;

  if (!fn_sig.empty()) {
    std::string_view args(fn_sig);
    std::string_view return_type(fn_sig);
    const size_t args_beg_pos = fn_sig.find('(');
    const size_t args_end_pos = fn_sig.find(')');
    const auto l = args.length();
    args.remove_prefix(1 + args_beg_pos);
    args.remove_suffix(l - args_end_pos);
    return_type.remove_prefix(1 + args_end_pos);
    demangled_type = DemangleKernel(return_type);
    demangled_args = DemangleKernel(args);
  }
  if (!class_sig.empty()) {
    demangled_class = DemangleKernel(class_sig);
  }

  return absl::StrCat(demangled_type, " ", demangled_class, "::", sym, "(", demangled_args, ")");
}

}  // namespace java
}  // namespace stirling
}  // namespace px
