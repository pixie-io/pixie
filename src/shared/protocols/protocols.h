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

namespace px {
namespace protocols {

// This is a list of protocols supported by Pixie.
// WARNING: Changes to this enum are API-breaking.
// You may add a protocol, but do not change values for existing protocols,
// and do not remove any protocols.
enum class Protocol {
  kUnknown = 0,
  kHTTP = 1,
  kHTTP2 = 2,
  kMySQL = 3,
  kCQL = 4,
  kPGSQL = 5,
  kDNS = 6,
  kRedis = 7,
  kNATS = 8,
  kMongo = 9,
  kKafka = 10,
  kMux = 11,
  kAMQP = 12,
};

}  // namespace protocols
}  // namespace px
