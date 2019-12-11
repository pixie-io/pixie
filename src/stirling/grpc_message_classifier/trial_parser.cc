#include "src/stirling/grpc_message_classifier/trial_parser.h"

#include <absl/memory/memory.h>

#include <string>

#include "src/common/base/logging.h"

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::Message;

StatusOr<std::unique_ptr<Message>> ParseAs(pl::grpc::ServiceDescriptorDatabase* desc_db,
                                           const std::string& message_type_name,
                                           const std::string& message, ParseAsOpts opts) {
  std::unique_ptr<Message> message_obj = desc_db->GetMessage(message_type_name);
  if (message_obj == nullptr) {
    return error::NotFound("Could not find message type with name $0", message_type_name);
  }
  message_obj->pl_allow_repeated_opt_fields_ = opts.allow_repeated_opt_fields;
  bool success = message_obj->ParseFromString(message);
  if (!success) {
    return std::unique_ptr<Message>(nullptr);
  }

  if (!opts.allow_unknown_fields) {
    // Unknown fields are very broad in definition, including:
    //  - Field with wrong wire_type.
    //  - Duplicate field numbers.
    //  - Unknown field numbers.
    // TODO(oazizi): An unknown field number is semantically different than the rest.
    //               Wrong wire_type and a duplicate field numbers are arguably malformed messages,
    //               while an unknown field number is still parseable.
    //               It would be nice if this function could treat the cases separately.
    //               (e.g. differentiate unknown_fields vs inconsistent_fields).

    int orig_size = message_obj->ByteSize();
    message_obj->DiscardUnknownFields();
    if (message_obj->ByteSize() != orig_size) {
      return std::unique_ptr<Message>(nullptr);
    }

    // Alternate method of checking, for reference:
    //    const google::protobuf::Reflection* refl = message_obj->GetReflection();
    //    if (refl->GetUnknownFields(message_obj).field_count() != 0) {
    //      return std::unique_ptr<Message>(nullptr);
    //    }
  }

  return message_obj;
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
