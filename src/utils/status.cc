#include "src/utils/status.h"

#include <cassert>
#include <string>

#include "src/utils/error_strings.h"

namespace pl {

Status::Status(pl::error::Code code, const std::string& msg) {
  assert(code != pl::error::OK);
  state_ = std::make_unique<State>();
  state_->code = code;
  state_->msg = msg;
}

std::string Status::ToString() const {
  if (ok()) {
    return "OK";
  }
  return pl::error::CodeToString(code()) + " : " + state_->msg;
}

}  // namespace pl
