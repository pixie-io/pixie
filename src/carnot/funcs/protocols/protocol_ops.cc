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

#include "src/carnot/funcs/protocols/protocol_ops.h"

#include "src/carnot/funcs/protocols/amqp.h"
#include "src/carnot/funcs/protocols/cql.h"
#include "src/carnot/funcs/protocols/dns.h"
#include "src/carnot/funcs/protocols/http.h"
#include "src/carnot/funcs/protocols/kafka.h"
#include "src/carnot/funcs/protocols/mux.h"
#include "src/carnot/funcs/protocols/mysql.h"
#include "src/carnot/funcs/protocols/protocols.h"
#include "src/carnot/udf/registry.h"
#include "src/common/base/base.h"

namespace px {
namespace carnot {
namespace funcs {
namespace protocols {

void RegisterProtocolOpsOrDie(px::carnot::udf::Registry* registry) {
  CHECK(registry != nullptr);
  /*****************************************
   * Scalar UDFs.
   *****************************************/
  registry->RegisterOrDie<ProtocolNameUDF>("protocol_name");
  registry->RegisterOrDie<HTTPRespMessageUDF>("http_resp_message");
  registry->RegisterOrDie<KafkaAPIKeyNameUDF>("kafka_api_key_name");
  registry->RegisterOrDie<MySQLCommandNameUDF>("mysql_command_name");
  registry->RegisterOrDie<CQLOpcodeNameUDF>("cql_opcode_name");
  registry->RegisterOrDie<AMQPFrameTypeUDF>("amqp_frame_type_name");
  registry->RegisterOrDie<AMQPMethodTypeUDF>("amqp_method_name");
  registry->RegisterOrDie<MuxFrameTypeUDF>("mux_frame_type_name");
  registry->RegisterOrDie<DNSRcodeNameUDF>("dns_rcode_name");
}

types::StringValue ProtocolNameUDF::Exec(FunctionContext*, Int64Value protocol) {
  return ProtocolName(protocol.val);
}

types::StringValue HTTPRespMessageUDF::Exec(FunctionContext*, Int64Value resp_code) {
  return http::RespCodeToMessage(resp_code.val);
}

types::StringValue KafkaAPIKeyNameUDF::Exec(FunctionContext*, Int64Value api_key) {
  return kafka::APIKeyName(api_key.val);
}

types::StringValue AMQPFrameTypeUDF::Exec(FunctionContext*, Int64Value frame_type) {
  return amqp::FrameTypeName(frame_type.val);
}

types::StringValue AMQPMethodTypeUDF::Exec(FunctionContext*, Int64Value class_id,
                                           Int64Value method_id) {
  return amqp::ClassIdMethodIdToMethodName(class_id.val, method_id.val);
}

types::StringValue MySQLCommandNameUDF::Exec(FunctionContext*, Int64Value api_key) {
  return mysql::CommandName(api_key.val);
}

types::StringValue CQLOpcodeNameUDF::Exec(FunctionContext*, Int64Value req_op) {
  return cql::RequestOpcodeToName(req_op.val);
}

types::StringValue MuxFrameTypeUDF::Exec(FunctionContext*, Int64Value frame_type) {
  return mux::FrameTypeName(frame_type.val);
}

types::StringValue DNSRcodeNameUDF::Exec(FunctionContext*, Int64Value rcode) {
  return dns::RcodeToName(rcode.val);
}

}  // namespace protocols
}  // namespace funcs
}  // namespace carnot
}  // namespace px
