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

#include "src/common/base/status.h"

#include <string>

#include <absl/strings/str_cat.h>

#include "src/common/base/error.h"

namespace px {

Status::Status(statuspb::Code code, const std::string& msg) {
  state_ = std::make_unique<State>();
  state_->code = code;
  state_->msg = msg;
}

Status::Status(statuspb::Code code, const std::string& msg,
               std::unique_ptr<google::protobuf::Message> context) {
  state_ = std::make_unique<State>(code, msg, std::move(context));
}

Status::Status(const px::statuspb::Status& status_pb) {
  if (status_pb.err_code() == statuspb::Code::OK) {
    return;
  }
  std::unique_ptr<google::protobuf::Any> context = nullptr;
  // If type_url().empty() is true, then the Any field is not initialized
  // and we can skip reading it.
  if (!status_pb.context().type_url().empty()) {
    context = std::make_unique<google::protobuf::Any>();
    context->set_type_url(status_pb.context().type_url());
    *(context->mutable_value()) = status_pb.context().value();
  }
  state_ = std::make_unique<State>(status_pb.err_code(), status_pb.msg(), std::move(context));
}

std::string Status::ToString() const {
  if (ok()) {
    return "OK";
  }
  std::string context_str;
  if (has_context()) {
    context_str = " Context: ";
    context_str += context()->DebugString();
  }
  return absl::StrCat(px::error::CodeToString(code()), " : ", state_->msg + context_str);
}

px::statuspb::Status Status::ToProto() const {
  px::statuspb::Status spb;
  ToProto(&spb);
  return spb;
}

void Status::ToProto(px::statuspb::Status* status_pb) const {
  CHECK(status_pb != nullptr);
  if (state_ == nullptr) {
    status_pb->set_err_code(statuspb::Code::OK);
    return;
  }
  status_pb->set_msg(state_->msg);
  status_pb->set_err_code(state_->code);
  if (state_->context != nullptr) {
    auto context_pb = status_pb->mutable_context();
    // Note: this is an explicity copy, otherwise you get nested Any messages.
    context_pb->set_type_url(state_->context->type_url());
    *(context_pb->mutable_value()) = state_->context->value();
  }
}

}  // namespace px
