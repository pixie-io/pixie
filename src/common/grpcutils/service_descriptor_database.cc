#include "src/common/grpcutils/service_descriptor_database.h"

#include <string>

#include "absl/memory/memory.h"
#include "src/common/base/logging.h"

namespace pl {
namespace grpc {

using ::google::protobuf::DescriptorPool;
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

}  // namespace grpc
}  // namespace pl
