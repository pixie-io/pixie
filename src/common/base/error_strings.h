#pragma once

#include <string>

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/statuspb/status.pb.h"

namespace px {
namespace error {

std::string CodeToString(px::statuspb::Code code);

}  // namespace error
}  // namespace px
