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

#include "experimental/stirling/proto_data_model/http_record.h"

#include <utility>

#include "experimental/stirling/proto_data_model/proto/http_record.pb.h"
#include "src/shared/types/column_wrapper.h"

namespace experimental {

using ::experimental::HTTPRecord;
using ::google::protobuf::FieldDescriptor;
using ::px::types::ColumnWrapperRecordBatch;

void ConsumeHTTPRecord(HTTPRecord record, ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;
#define ASSIGN(field_name, field_name_alias, type)                                    \
  columns[HTTPRecord::k##field_name_alias##FieldNumber - 1]->Append<px::types::type>( \
      record.field_name())
#define ASSIGN_STR(field_name, field_name_alias)                                             \
  columns[HTTPRecord::k##field_name_alias##FieldNumber - 1]->Append<px::types::StringValue>( \
      std::move(*record.mutable_##field_name()))
  ASSIGN(time_stamp_ns, TimeStampNs, Time64NSValue);
  ASSIGN(tgid, Tgid, Int64Value);
  ASSIGN(fd, Fd, Int64Value);
  ASSIGN_STR(type, Type);
  ASSIGN_STR(src_addr, SrcAddr);
  ASSIGN(src_port, SrcPort, Int64Value);
  ASSIGN_STR(dst_addr, DstAddr);
  ASSIGN(dst_port, DstPort, Int64Value);
  ASSIGN(minor_version, MinorVersion, Int64Value);
  ASSIGN_STR(headers, Headers);
  ASSIGN_STR(req_method, ReqMethod);
  ASSIGN_STR(req_path, ReqPath);
  ASSIGN(resp_status, RespStatus, Int64Value);
  ASSIGN_STR(resp_message, RespMessage);
  ASSIGN_STR(resp_body, RespBody);
  ASSIGN(resp_latency_ns, RespLatencyNs, Int64Value);
}

void ConsumeHTTPRecordRefl(HTTPRecord record, ColumnWrapperRecordBatch* record_batch) {
  auto& columns = *record_batch;
  auto* desc = record.GetDescriptor();
  auto* refl = record.GetReflection();
  // There is not PB CPP type that matches Time64NSValue, so we just manually populate.
  columns[0]->Append<px::types::Time64NSValue>(refl->GetUInt64(record, desc->field(0)));
  for (int i = 1; i < desc->field_count(); i++) {
    auto* field_desc = desc->field(i);
    switch (field_desc->cpp_type()) {
      case FieldDescriptor::CPPTYPE_INT32:
        columns[field_desc->number() - 1]->Append<px::types::Int64Value>(
            refl->GetInt32(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_INT64:
        columns[field_desc->number() - 1]->Append<px::types::Int64Value>(
            refl->GetInt64(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_UINT32:
        columns[field_desc->number() - 1]->Append<px::types::Int64Value>(
            refl->GetUInt32(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_UINT64:
        columns[field_desc->number() - 1]->Append<px::types::Int64Value>(
            refl->GetUInt64(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_DOUBLE:
        columns[field_desc->number() - 1]->Append<px::types::Float64Value>(
            refl->GetDouble(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_FLOAT:
        columns[field_desc->number() - 1]->Append<px::types::Float64Value>(
            refl->GetFloat(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_BOOL:
        columns[field_desc->number() - 1]->Append<px::types::BoolValue>(
            refl->GetBool(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_STRING:
        columns[field_desc->number() - 1]->Append<px::types::StringValue>(
            refl->GetString(record, field_desc));
        break;
      case FieldDescriptor::CPPTYPE_ENUM:
      case FieldDescriptor::CPPTYPE_MESSAGE:
        CHECK(false) << "Does not support message.";
        break;
    }
  }
}

}  // namespace experimental
