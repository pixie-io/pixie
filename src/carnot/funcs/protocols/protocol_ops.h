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

class ProtocolNameUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value resp_code);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a protocol code to its corresponding name.")
        .Details(
            "UDF to convert the internal Pixie protocol numbers into their corresponding protocol "
            "names.")
        .Arg("protocol", "A protocol code")
        .Example("df.protocol_name = px.protocol_name(df.protocol)")
        .Returns("The name of the protocol.");
  }
};

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

class CQLOpcodeNameUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value req_op);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a CQL op code to its name.")
        .Details("UDF to convert CQL request opcodes into their corresponding names.")
        .Arg("req_op", "A CQL opcode")
        .Example("df.cmd = px.cql_opcode_name(df.req_op)")
        .Returns("The request opcode's name.");
  }
};

class AMQPFrameTypeUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value frame_type);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a AMQP frame type to its name.")
        .Details("UDF to convert AMQP frame type into their corresponding human-readable names.")
        .Arg("frame_type", "An AMQP frame_type in numeric value")
        .Example("df.frame_type_name = px.amqp_frame_type_name(df.req_cmd)")
        .Returns("The AMQP Frame Type name.");
  }
};

class AMQPMethodTypeUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value class_id, Int64Value method_id);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a AMQP method id to its name.")
        .Details("UDF to convert AMQP method id into their corresponding human-readable names.")
        .Arg("class_id", "An AMQP class_id in numeric value")
        .Arg("method_id", "An AMQP method_id in numeric value")
        .Example("df.method_name = px.amqp_method_name(df.class_id, df.method_id)")
        .Returns("The AMQP Method name.");
  }
};

class MuxFrameTypeUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value frame_type);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a Mux frame type to its name.")
        .Details("UDF to convert Mux frame type into their corresponding human-readable names.")
        .Arg("frame_type", "A Mux frame_type in numeric value")
        .Example("df.frame_type_name = px.mux_frame_type_name(df.req_cmd)")
        .Returns("The mux frame type name.");
  }
};

class DNSRcodeNameUDF : public px::carnot::udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Int64Value rcode);

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Convert a DNS rcode to its name.")
        .Details("UDF to convert DNS rcodes into their corresponding names.")
        .Arg("rcode", "A DNS rcode")
        .Example("df.rcode_name = px.dns_rcode_name(df.rcode)")
        .Returns("The request opcode's name.");
  }
};

void RegisterProtocolOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
