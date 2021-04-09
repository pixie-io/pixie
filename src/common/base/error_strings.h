#pragma once

#include <string>

#include <absl/strings/str_cat.h>
#include <absl/strings/substitute.h>

#include "src/common/base/statuspb/status.pb.h"

namespace pl {
namespace error {

std::string CodeToString(pl::statuspb::Code code);

}  // namespace error
}  // namespace pl
