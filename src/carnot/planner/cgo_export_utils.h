#pragma once
#include <memory>
#include <string>

#include "src/common/base/error.h"
#include "src/common/base/status.h"

char* CloneStringToCharArray(std::string str, int* ret_len) {
  *ret_len = str.size();
  char* retval = new char[str.size()];
  memcpy(retval, str.data(), str.size());
  return retval;
}
char* PrepareResult(google::protobuf::Message* pb, int* result_len) {
  DCHECK(pb);
  std::string serialized;
  bool success = pb->SerializeToString(&serialized);

  if (!success) {
    *result_len = 0;
    return nullptr;
  }
  return CloneStringToCharArray(serialized, result_len);
}
// TMessage should be a proto type with a Status message.
template <typename TMessage>
void WrapStatus(TMessage* pb, const px::Status& status) {
  DCHECK(pb);
  status.ToProto(pb->mutable_status());
}

template <typename TMessage>
char* ExitEarly(const px::Status& status, int* result_len) {
  DCHECK(result_len != nullptr);
  TMessage pb;
  WrapStatus<TMessage>(&pb, status);
  return PrepareResult(&pb, result_len);
}

template <typename TMessage>
char* ExitEarly(const std::string& err, int* result_len) {
  return ExitEarly<TMessage>(px::error::InvalidArgument(err), result_len);
}
