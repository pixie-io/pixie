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

#include <absl/container/flat_hash_set.h>
#include <string>
#include <utility>

#include "src/common/testing/testing.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/pgsql/types.h"

namespace px {
namespace stirling {
namespace protocols {
namespace pgsql {

TEST(TagToString, AllTagsHaveToString) {
  absl::flat_hash_set<Tag> req_tags = {
      Tag::kCopyData, Tag::kCopyDone,  Tag::kQuery,  Tag::kCopyFail, Tag::kClose,
      Tag::kBind,     Tag::kPasswd,    Tag::kParse,  Tag::kDesc,     Tag::kSync,
      Tag::kExecute,  Tag::kTerminate, Tag::kUnknown};
  absl::flat_hash_set<Tag> resp_tags = {
      Tag::kCopyData,      Tag::kCopyDone,        Tag::kDataRow,
      Tag::kReadyForQuery, Tag::kCopyOutResponse, Tag::kCopyInResponse,
      Tag::kErrResp,       Tag::kCmdComplete,     Tag::kEmptyQueryResponse,
      Tag::kCloseComplete, Tag::kBindComplete,    Tag::kKey,
      Tag::kAuth,          Tag::kParseComplete,   Tag::kParamDesc,
      Tag::kParamStatus,   Tag::kRowDesc,         Tag::kNoData,
      Tag::kUnknown};

  auto tag_values = magic_enum::enum_values<Tag>();
  for (auto tag : tag_values) {
    if (req_tags.contains(tag)) {
      // make sure no DCHECK is hit.
      ToString(tag, /* is_req */ true);
    }
    if (resp_tags.contains(tag)) {
      // make sure no DCHECK is hit.
      ToString(tag, /* is_req */ false);
    }
    if (!req_tags.contains(tag) && !resp_tags.contains(tag)) {
      EXPECT_TRUE(false) << "Tag " << magic_enum::enum_name(tag)
                         << " is not in req_tags or resp_tags.";
    }
  }
}

}  // namespace pgsql
}  // namespace protocols
}  // namespace stirling
}  // namespace px
