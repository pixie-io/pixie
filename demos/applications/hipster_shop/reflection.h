#pragma once

#include <google/protobuf/descriptor.pb.h>

namespace demos {
namespace hipster_shop {

/**
 * @brief Returns a FileDescriptorSet protobuf for all of Hipster Shop services and their messages.
 */
google::protobuf::FileDescriptorSet GetFileDescriptorSet();

}  // namespace hipster_shop
}  // namespace demos
