#include "src/common/base/status.h"

#include <string>

#include "src/common/base/error_strings.h"

namespace pl {

Status::Status(statuspb::Code code, const std::string& msg) {
  state_ = std::make_unique<State>();
  state_->code = code;
  state_->msg = msg;
}

Status::Status(const pl::statuspb::Status& status_pb) {
  if (status_pb.err_code() == statuspb::Code::OK) {
    return;
  }
  *this = Status(status_pb.err_code(), status_pb.msg());
}

std::string Status::ToString() const {
  if (ok()) {
    return "OK";
  }
  return pl::error::CodeToString(code()) + " : " + state_->msg;
}

pl::statuspb::Status Status::ToProto() const {
  pl::statuspb::Status spb;
  ToProto(&spb);
  return spb;
}

void Status::ToProto(pl::statuspb::Status* status_pb) const {
  CHECK(status_pb != nullptr);
  if (state_ == nullptr) {
    status_pb->set_err_code(statuspb::Code::OK);
    return;
  }
  status_pb->set_msg(state_->msg);
  status_pb->set_err_code(state_->code);
}

}  // namespace pl
