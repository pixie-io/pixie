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

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/vizier/funcs/funcs.h"

DEFINE_string(out_file_path, gflags::StringFromEnv("PL_FUNCS_OUT_FILE", "funcs.pb"),
              "The file to save the serialized UDFInfo");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);
  px::carnot::udf::Registry registry("registry");
  px::vizier::funcs::VizierFuncFactoryContext ctx;
  RegisterFuncsOrDie(ctx, &registry);

  // Serialzie the registry as a protobuf
  std::ofstream out_udf_info;
  out_udf_info.open(FLAGS_out_file_path);

  px::carnot::udfspb::UDFInfo udf_info = registry.ToProto();
  udf_info.SerializeToOstream(&out_udf_info);

  out_udf_info.close();

  return 0;
}
