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
#include <memory>
#include <string>

#include "src/common/base/error.h"
#include "src/common/base/status.h"

char* CloneStringToCharArray(std::string str, int* ret_len) {
  *ret_len = str.size();
  char* retval = new char[str.size()];
  memcpy(retval, str.data(), str.size());
  return retval;
}
char* PrepareResult(google::protobuf::Message* pb, int* result_len) {
  DCHECK(pb);
  std::string serialized;
  bool success = pb->SerializeToString(&serialized);

  if (!success) {
    *result_len = 0;
    return nullptr;
  }
  return CloneStringToCharArray(serialized, result_len);
}
// TMessage should be a proto type with a Status message.
template <typename TMessage>
void WrapStatus(TMessage* pb, const px::Status& status) {
  DCHECK(pb);
  status.ToProto(pb->mutable_status());
}

template <typename TMessage>
char* ExitEarly(const px::Status& status, int* result_len) {
  DCHECK(result_len != nullptr);
  TMessage pb;
  WrapStatus<TMessage>(&pb, status);
  return PrepareResult(&pb, result_len);
}

template <typename TMessage>
char* ExitEarly(const std::string& err, int* result_len) {
  return ExitEarly<TMessage>(px::error::InvalidArgument(err), result_len);
}
