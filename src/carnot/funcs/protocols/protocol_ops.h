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

#include "src/carnot/funcs/protocols/http.h"
#include "src/carnot/funcs/protocols/kafka.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {

class HTTPRespMessageUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value resp_code) {
    return http::HTTPRespCodeToMessage(resp_code.val);
  }

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
  StringValue Exec(FunctionContext*, Int64Value api_key) {
    return kafka::KafkaAPIKeyName(api_key.val);
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a Kafka API key to its name.")
        .Details("UDF to convert Kafka API keys into their corresponding human-readable names.")
        .Arg("api_key", "A Kafka API key")
        .Example("df.api_key_name = px.kafka_api_key_name(df.req_cmd)")
        .Returns("The API key's name.");
  }
};

void RegisterProtocolOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
