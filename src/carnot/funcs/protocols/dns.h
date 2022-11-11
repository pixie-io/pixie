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
namespace dns {

inline std::string RcodeToName(int rcode) {
  switch (rcode) {
    case 0:
      return "NOERROR";
    case 1:
      return "FORMERR";
    case 2:
      return "SERVFAIL";
    case 3:
      return "NXDOMAIN";
    case 4:
      return "NOTIMP";
    case 5:
      return "REFUSED";
    case 6:
      return "YXDOMAIN";
    case 7:
      return "YXRRSET";
    case 8:
      return "NXRRSET";
    case 9:
      return "NOTAUTH";
    case 10:
      return "NOTZONE";
    case 11:
      return "DSOTYPENI";
    case 16:
      return "BADVERS";
    case 17:
      return "BADKEY";
    case 18:
      return "BADTIME";
    case 19:
      return "BADMODE";
    case 20:
      return "BADNAME";
    case 21:
      return "BADALG";
    case 22:
      return "BADTRUNC";
    case 23:
      return "BADCOOKIE";
    default:
      return std::to_string(rcode);
  }
}

}  // namespace dns

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
