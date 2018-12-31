#pragma once

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"

#include "src/utils/codes/error_codes.pb.h"

namespace pl {
namespace error {

std::string CodeToString(pl::error::Code code);

}  // namespace error
}  // namespace pl
