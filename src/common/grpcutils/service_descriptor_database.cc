#include "src/common/grpcutils/service_descriptor_database.h"

#include <string>

#include "absl/memory/memory.h"
#include "src/common/base/logging.h"

namespace pl {
namespace grpc {

using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::MethodDescriptor;
using ::google::protobuf::ServiceDescriptor;
using ::google::protobuf::SimpleDescriptorDatabase;

ServiceDescriptorDatabase::ServiceDescriptorDatabase(FileDescriptorSet fdset)
    : desc_db_(), desc_pool_(&desc_db_), message_factory_() {
  for (const auto& fd : fdset.file()) {
    if (!desc_db_.Add(fd)) {
      LOG(ERROR) << "Failed to insert FileDescriptorProto for file: " << fd.name();
    }
  }
}

MethodInputOutput ServiceDescriptorDatabase::GetMethodInputOutput(const std::string& method_path) {
  const MethodDescriptor* method = desc_pool_.FindMethodByName(method_path);
  if (method == nullptr) {
    return {nullptr, nullptr};
  }

  MethodInputOutput res;

  const Message* input_prototype = message_factory_.GetPrototype(method->input_type());
  if (input_prototype != nullptr) {
    res.input.reset(input_prototype->New());
  }
  const Message* output_prototype = message_factory_.GetPrototype(method->output_type());
  if (output_prototype != nullptr) {
    res.output.reset(output_prototype->New());
  }
  return res;
}

std::unique_ptr<Message> ServiceDescriptorDatabase::GetMessage(const std::string& message_path) {
  const Descriptor* msg_desc = desc_pool_.FindMessageTypeByName(message_path);
  if (msg_desc == nullptr) {
    return nullptr;
  }
  const Message* msg_prototype = message_factory_.GetPrototype(msg_desc);
  if (msg_prototype == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<Message>(msg_prototype->New());
}

std::vector<::google::protobuf::ServiceDescriptorProto> ServiceDescriptorDatabase::AllServices() {
  std::vector<google::protobuf::ServiceDescriptorProto> services;

  std::vector<std::string> filenames;
  desc_db_.FindAllFileNames(&filenames);
  for (auto& filename : filenames) {
    FileDescriptorProto desc_proto;
    desc_db_.FindFileByName(filename, &desc_proto);
    for (const auto& service : desc_proto.service()) {
      services.push_back(service);
    }
  }
  return services;
}

// TODO(oazizi): Consider moving this out of this file, perhaps to MessageGroupTypeClassifier.
StatusOr<std::unique_ptr<Message>> ParseAs(ServiceDescriptorDatabase* desc_db,
                                           const std::string& message_type_name,
                                           const std::string& message, bool allow_unknown_fields) {
  std::unique_ptr<Message> message_obj = desc_db->GetMessage(message_type_name);
  if (message_obj == nullptr) {
    return error::NotFound("Could not find message type with name $0", message_type_name);
  }
  bool success = message_obj->ParseFromString(message);
  if (!success) {
    return std::unique_ptr<Message>(nullptr);
  }

  if (!allow_unknown_fields) {
    // Unknown fields are very broad in definition, including:
    //  - Field with wrong wire_type.
    //  - Duplicate field numbers.
    //  - Unknown field numbers.
    // TODO(oazizi): An unknown field number is semantically different than the rest.
    //               Wrong wire_type and a duplicate field numbers are arguably malformed messages,
    //               while an unknown field number is still parseable.
    //               It would be nice if this function could treat the cases separately.
    //               (e.g. differentiate unknown_fields vs inconsistent_fields).

    int orig_size = message_obj->ByteSize();
    message_obj->DiscardUnknownFields();
    if (message_obj->ByteSize() != orig_size) {
      return std::unique_ptr<Message>(nullptr);
    }

    // Alternate method of checking, for reference:
    //    const google::protobuf::Reflection* refl = message_obj->GetReflection();
    //    if (refl->GetUnknownFields(message_obj).field_count() != 0) {
    //      return std::unique_ptr<Message>(nullptr);
    //    }
  }

  return message_obj;
}

}  // namespace grpc
}  // namespace pl
