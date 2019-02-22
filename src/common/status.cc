#include "src/common/status.h"

#include <cassert>
#include <string>

#include "src/common/error_strings.h"

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

pl::statuspb::Status Status::ToProto() {
  pl::statuspb::Status spb;
  if (state_ == nullptr) {
    spb.set_err_code(error::Code::OK);
    return spb;
  }
  spb.set_msg(state_->msg);
  spb.set_err_code(state_->code);
  return spb;
}

}  // namespace pl
