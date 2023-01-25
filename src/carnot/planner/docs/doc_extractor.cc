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

#include "src/carnot/planner/docs/doc_extractor.h"

namespace px {
namespace carnot {
namespace planner {
namespace docs {

Status DocHolder::ToProto(docspb::DocstringNode* pb) const {
  pb->set_name(name);
  pb->set_docstring(doc);
  for (const auto& [k, v] : attrs) {
    PX_UNUSED(k);
    PX_RETURN_IF_ERROR(v.ToProto(pb->add_children()));
  }
  return Status::OK();
}

DocHolder DocExtractor::ExtractDoc(const compiler::QLObjectPtr& qobject) {
  DocHolder doc;
  doc.name = qobject->name();
  doc.doc = qobject->doc_string();
  for (const auto& [k, v] : qobject->attributes()) {
    doc.attrs[k] = ExtractDoc(v);
  }

  for (const auto& [k, v] : qobject->methods()) {
    doc.attrs[k] = ExtractDoc(v);
  }
  return doc;
}

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace px
