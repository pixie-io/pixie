#include "src/stirling/testing/demo_apps/hipster_shop/reflection.h"

#include "src/stirling/testing/demo_apps/hipster_shop/proto/demo.pb.h"

namespace demos {
namespace hipster_shop {

using ::google::protobuf::FileDescriptorSet;

FileDescriptorSet GetFileDescriptorSet() {
  FileDescriptorSet res;
  // FileDescriptor can be obtained from any message defined in the file.
  // Here CartItem is of no particularly meaning.
  hipstershop::CartItem::descriptor()->file()->CopyTo(res.add_file());
  return res;
}

}  // namespace hipster_shop
}  // namespace demos
