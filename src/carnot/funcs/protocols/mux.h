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

#include <string>

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {
namespace mux {

inline std::string FrameTypeName(int frame_type) {
  switch (frame_type) {
    case 1:
      return "Treq";
    case -1:
      return "Rreq";
    case 2:
      return "Tdispatch";
    case -2:
      return "Rdispatch";
    case 64:
      return "Tdrain";
    case -64:
      return "Rdrain";
    case 65:
      return "Tping";
    case -65:
      return "Rping";
    case 66:
      return "Tdiscarded";
    case -66:
      return "Rdiscarded";
    case 67:
      return "Tlease";
    case 68:
      return "Tinit";
    case -68:
      return "Rinit";
    case -128:
      return "Rerr";
    case 127:
      return "Rerr (legacy)";
    case -62:
      return "Tdiscarded (legacy)";
    default:
      return absl::Substitute("Unknown ($0)", frame_type);
  }
}

}  // namespace mux

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
