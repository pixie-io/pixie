#pragma once

#include <google/protobuf/descriptor.pb.h>

#include "src/stirling/testing/proto/greet.pb.h"

namespace pl {
namespace stirling {
namespace testing {

inline google::protobuf::FileDescriptorSet GreetServiceFDSet() {
  google::protobuf::FileDescriptorSet res;
  HelloReply::descriptor()->file()->CopyTo(res.add_file());
  return res;
}

}  // namespace testing
}  // namespace stirling
}  // namespace pl
