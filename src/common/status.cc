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
  ToProto(&spb);
  return spb;
}

void Status::ToProto(pl::statuspb::Status* status_pb) {
  CHECK(status_pb != nullptr);
  if (state_ == nullptr) {
    status_pb->set_err_code(error::Code::OK);
    return;
  }
  status_pb->set_msg(state_->msg);
  status_pb->set_err_code(state_->code);
}

}  // namespace pl
