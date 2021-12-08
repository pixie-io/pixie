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

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {

class HTTPRespMessageUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value resp_code);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert an HTTP response code to its corresponding message.")
        .Details("UDF to convert HTTP response codes into their corresponding messages.")
        .Arg("code", "An HTTP response code (e.g. 404)")
        .Example("df.resp_message = px.http_resp_message(df.resp_status)")
        .Returns("The HTTP response message.");
  }
};

class KafkaAPIKeyNameUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value api_key);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a Kafka API key to its name.")
        .Details("UDF to convert Kafka API keys into their corresponding human-readable names.")
        .Arg("api_key", "A Kafka API key")
        .Example("df.api_key_name = px.kafka_api_key_name(df.req_cmd)")
        .Returns("The API key's name.");
  }
};

class MySQLCommandNameUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value api_key);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a MySQL command code to its name.")
        .Details("UDF to convert MySQL request command codes into their corresponding names.")
        .Arg("cmd", "A MySQL command code")
        .Example("df.cmd = px.mysql_command_name(df.req_cmd)")
        .Returns("The request code's name.");
  }
};

void RegisterProtocolOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
