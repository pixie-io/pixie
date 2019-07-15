#pragma once

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>

#include <memory>
#include <string>
#include <utility>

namespace pl {
namespace grpc {

struct MethodInputOutput {
  std::unique_ptr<google::protobuf::Message> input;
  std::unique_ptr<google::protobuf::Message> output;
};

/**
 * @brief Indexes services and the descriptors of their methods' input and output protobuf messages.
 */
class ServiceDescriptorDatabase {
 public:
  explicit ServiceDescriptorDatabase(google::protobuf::FileDescriptorSet fdset);

  /**
   * @brief Returns empty instances of the input and output type of the method specified by the
   * input method path.
   *
   * @param method_path A dot-separated name including the service name.
   */
  MethodInputOutput GetMethodInputOutput(const std::string& method_path);

 private:
  google::protobuf::SimpleDescriptorDatabase desc_db_;
  google::protobuf::DescriptorPool desc_pool_;
  google::protobuf::DynamicMessageFactory message_factory_;
};

// TODO(yzhao): Benchmark dynamic message parsing.

}  // namespace grpc
}  // namespace pl
