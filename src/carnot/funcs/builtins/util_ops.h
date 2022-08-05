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

#include <grpcpp/grpcpp.h>

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace builtins {

class GRPCStatusToStringUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value status_code) {
    ::grpc::StatusCode code = static_cast<::grpc::StatusCode>(status_code.val);
    switch (code) {
      case ::grpc::StatusCode::OK:
        return "OK";
      case ::grpc::StatusCode::CANCELLED:
        return "CANCELLED";
      case ::grpc::StatusCode::UNKNOWN:
        return "UNKNOWN";
      case ::grpc::StatusCode::INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
      case ::grpc::StatusCode::DEADLINE_EXCEEDED:
        return "DEADLINE_EXCEEDED";
      case ::grpc::StatusCode::NOT_FOUND:
        return "NOT_FOUND";
      case ::grpc::StatusCode::ALREADY_EXISTS:
        return "ALREADY_EXISTS";
      case ::grpc::StatusCode::PERMISSION_DENIED:
        return "PERMISSION_DENIED";
      case ::grpc::StatusCode::UNAUTHENTICATED:
        return "UNAUTHENTICATED";
      case ::grpc::StatusCode::RESOURCE_EXHAUSTED:
        return "RESOURCE_EXHAUSTED";
      case ::grpc::StatusCode::FAILED_PRECONDITION:
        return "FAILED_PRECONDITION";
      case ::grpc::StatusCode::ABORTED:
        return "ABORTED";
      case ::grpc::StatusCode::OUT_OF_RANGE:
        return "OUT_OF_RANGE";
      case ::grpc::StatusCode::UNIMPLEMENTED:
        return "UNIMPLEMENTED";
      case ::grpc::StatusCode::INTERNAL:
        return "INTERNAL";
      case ::grpc::StatusCode::UNAVAILABLE:
        return "UNAVAILABLE";
      case ::grpc::StatusCode::DATA_LOSS:
        return "DATA_LOSS";
      case ::grpc::StatusCode::DO_NOT_USE:
        return "DO_NOT_USE";
    }
    return "unknown code";
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Converts the GRPC status code into a readable string error.")
        .Example(R"doc(
        | df.err_reason = px.grpc_status_code_to_str(12)
        )doc")
        .Arg("status_code", "The GRPC status code to convert.")
        .Returns("A string representation of the GRPC status code.");
  }
};

void RegisterUtilOpsOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace px
