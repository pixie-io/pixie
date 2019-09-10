#pragma once

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/message.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/grpcutils/service_descriptor_database.h"

namespace pl {
namespace stirling {
namespace grpc {

struct ParseAsOpts {
  // Whether an unknown field is considered a parse failure.
  bool allow_unknown_fields;

  // Whether a duplicate value for a non-repeated field is considered a parse failure.
  bool allow_repeated_opt_fields;
};

constexpr ParseAsOpts kDefaultParseAsOpts = {.allow_unknown_fields = false,
                                             .allow_repeated_opt_fields = false};

/**
 * @brief Attempts to parse an instance of a protobuf message as the provided message type.
 *
 * @param message_type_name protobuf message type (e.g. hipstershop.GetCartRequest).
 * @param message a protobuf message in the wire format.
 * @return A unique_ptr to the decoded message if it was parseable, nullptr otherwise.
 *         An error is return for a unexpected cases or mis-use of the function;
 *         Currently an error is only returned if the message_type_name is not in the database.
 */
StatusOr<std::unique_ptr<google::protobuf::Message>> ParseAs(
    pl::grpc::ServiceDescriptorDatabase* desc_db, const std::string& message_type_name,
    const std::string& message, ParseAsOpts opts = kDefaultParseAsOpts);

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
