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
#include <arpa/inet.h>
#include <netdb.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/funcs/os/filesystem.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/common/base/inet_utils.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace os {

using ScalarUDF = px::carnot::udf::ScalarUDF;

class SharedLibrariesUDF : public ScalarUDF {
 public:
  static udfspb::UDFSourceExecutor Executor() { return udfspb::UDFSourceExecutor::UDF_PEM; }

  StringValue Exec(FunctionContext*, UInt128Value upid_value) {
    auto upid = md::UPID(upid_value.val);
    return internal::GetSharedLibraries(upid);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Get the shared libraries for a given pid.")
        .Details("Get the shared libraries for a given pid.")
        .Arg("pid", "The process ID")
        .Example("df.libraries = px.shared_libraries(df.pid)")
        .Returns("Stringified vector of shared libraries.");
  }
};

void RegisterProcessOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace os
}  // namespace funcs
}  // namespace carnot
}  // namespace px
