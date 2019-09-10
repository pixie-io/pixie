#include "src/stirling/grpc_message_classifier/message_matcher.h"

namespace google {
namespace protobuf {

bool operator<(const MethodDescriptorProto& a, const MethodDescriptorProto& b) {
  if (a.name() != b.name()) {
    return a.name() < b.name();
  }

  if (a.input_type() != b.input_type()) {
    return a.input_type() < b.input_type();
  }

  return a.output_type() < b.output_type();
}

}  // namespace protobuf
}  // namespace google

namespace pl {
namespace stirling {
namespace grpc {

using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::ServiceDescriptorProto;

using ::pl::Status;
using ::pl::StatusOr;

using ::pl::stirling::http2::HeaderField;
using ::pl::stirling::http2::IndexedHeaderField;
using ::pl::stirling::http2::IsInDynamicTable;
using ::pl::stirling::http2::IsInStaticTable;
using ::pl::stirling::http2::kStaticTableSize;
using ::pl::stirling::http2::LiteralHeaderField;
using ::pl::stirling::http2::ShouldUpdateDynamicTable;

//--------------------------------------------------------------
// MessageGroupTypeClassifier
//--------------------------------------------------------------

MessageGroupTypeClassifier::MessageGroupTypeClassifier(
    ::pl::grpc::ServiceDescriptorDatabase* desc_db, std::vector<std::string_view> service_names,
    ParseAsOpts msg_parse_opts)
    : desc_db_(desc_db), msg_parse_opts_(msg_parse_opts) {
  InitializeCandidateRPCs(service_names);
}

void MessageGroupTypeClassifier::AddMessage(const std::string& message, MessageDirection direction,
                                            std::string label) {
  // Check all the message against all RPCs still under consideration.
  matching_rpcs_ = FindMatchingRPCs(matching_rpcs_, message, direction);

  // Since this change the matching_rpcs_ set, add an event to the history.
  num_candidates_history_.push_back(matching_rpcs_.size());

  switch (direction) {
    case MessageDirection::kRequest:
      ++req_message_count;
      break;
    case MessageDirection::kResponse:
      ++resp_message_count;
      break;
    default:
      CHECK(false) << "Unexpected direction";
  }

  labels_.insert(std::move(label));
}

std::set<std::string> MessageGroupTypeClassifier::MatchingMessageTypes(
    MessageDirection direction) const {
  std::set<std::string> matching_message_types;

  switch (direction) {
    case MessageDirection::kRequest:
      for (auto& rpc : matching_rpcs_) {
        matching_message_types.insert(rpc.input_type());
      }
      break;
    case MessageDirection::kResponse:
      for (auto& rpc : matching_rpcs_) {
        matching_message_types.insert(rpc.output_type());
      }
      break;
    default:
      CHECK(false) << absl::Substitute("Unexpected message direction $0",
                                       static_cast<int>(direction));
  }

  return matching_message_types;
}

bool MessageGroupTypeClassifier::IsParseable(const std::string& message_type,
                                             const std::string& message) {
  StatusOr<std::unique_ptr<Message>> status_or_message;
  std::unique_ptr<Message> message_obj;

  // Drop the leading '.' character from the message type.
  // Example: ".hipstershop.AddItemRequest" -> "hipstershop.AddItemRequest"
  // This is required by desc_db_.ParseAs().
  status_or_message = ParseAs(desc_db_, message_type.substr(1), message, msg_parse_opts_);

  // If status is not okay, then we probably provided an non-existent message type.
  // Treat as not-parseable.
  if (!status_or_message.ok()) {
    return false;
  }

  // A nullptr indicates that message was not parseable as the provided message type.
  if (status_or_message.ValueOrDie() == nullptr) {
    return false;
  }

  return true;
}

std::set<::google::protobuf::MethodDescriptorProto> MessageGroupTypeClassifier::FindMatchingRPCs(
    const std::set<::google::protobuf::MethodDescriptorProto>& rpcs, const std::string& message,
    MessageDirection direction) {
  std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs;

  switch (direction) {
    case MessageDirection::kRequest:
      for (const auto& rpc : rpcs) {
        if (IsParseable(rpc.input_type(), message)) {
          matching_rpcs.insert(rpc);
        }
      }
      break;
    case MessageDirection::kResponse:
      for (const auto& rpc : rpcs) {
        if (IsParseable(rpc.output_type(), message)) {
          matching_rpcs.insert(rpc);
        }
      }
      break;
    case MessageDirection::kUnknown:
      LOG(DFATAL) << "Unknown message direction is not supported.";
      break;
  }

  return matching_rpcs;
}

void MessageGroupTypeClassifier::InitializeCandidateRPCs(
    std::vector<std::string_view> service_names) {
  std::vector<::google::protobuf::ServiceDescriptorProto> services = desc_db_->AllServices();

  matching_rpcs_.clear();
  num_candidates_history_.clear();

  for (const auto& service : services) {
    bool service_name_match = std::find(std::begin(service_names), std::end(service_names),
                                        service.name()) != std::end(service_names);
    if (service_names.empty() || service_name_match) {
      for (const auto& method : service.method()) {
        matching_rpcs_.insert(method);
      }
    }
  }

  num_candidates_history_.push_back(matching_rpcs_.size());
}

int MessageGroupTypeClassifier::NumMessages(MessageDirection direction) const {
  switch (direction) {
    case MessageDirection::kRequest:
      return req_message_count;
    case MessageDirection::kResponse:
      return resp_message_count;
    case MessageDirection::kUnknown:
      return req_message_count + resp_message_count;
    default:
      LOG(DFATAL) << "Unhandled case in NumMessages()";
      return 0;
  }
}

StatusOr<size_t> GRPCMessageFlow::AddMessage(const std::vector<HeaderField>& req_header_fields) {
  // Retrieve the first 2 non-static index to the dynamic table.
  constexpr int kReservedHeaderCount = 4;

  if (req_header_fields.size() < kReservedHeaderCount) {
    return error::InvalidArgument("There must be at least 4 header fields, but received only $0",
                                  req_header_fields.size());
  }

  // The two fields that could be :path and :authority, order is undetermined.
  std::vector<const HeaderField*> index_fields;
  for (int i = 0; i < kReservedHeaderCount; ++i) {
    const auto& field = req_header_fields[i];
    if (IsInStaticTable(field)) {
      continue;
    }
    // In case a field is plain text, we need to skip the one that is not :path & :authority.
    if (HoldsPlainTextName(field)) {
      std::string_view name = GetLiteralNameAsStringView(field);
      if (name != ":path" && name != ":authority") {
        continue;
      }
    }
    // TODO(yzhao): Address other corner cases:
    // 1. :path is plain text.
    // 2. Add huffman decoding (be careful about the statefulness of huffman decoding).
    index_fields.push_back(&field);
  }

  // TODO(yzhao): Change to use the first 4 header fields as index fields. Because we saw cases
  // where header fields that can be encoded as static table indexes are still encoded as dynamic
  // table indexes.
  if (index_fields.size() != 2) {
    return error::InvalidArgument(
        absl::StrCat("Expect exactly 2 relevant fields from the first 4 header fields, got ",
                     index_fields.size()));
  }

  const HeaderField& f1 = *index_fields[0];
  const HeaderField& f2 = *index_fields[1];

  if (!IsInDynamicTable(f1) && !ShouldUpdateDynamicTable(f1)) {
    return error::InvalidArgument(
        "The 1st header field is invalid, it must either be an indexed field "
        "with index in dynamic table, or a literal field with incremental indexing.");
  }
  if (!IsInDynamicTable(f2) && !ShouldUpdateDynamicTable(f2)) {
    return error::InvalidArgument(
        "The 2nd header field is invalid, it must either be an indexed field "
        "with index in dynamic table, or a literal field with incremental indexing.");
  }

  size_t res = 0;
  if (IsInDynamicTable(f1) && IsInDynamicTable(f2)) {
    res = AddMessageWithKey(std::make_pair(GetIndex(f1), GetIndex(f2)));
  } else if (IsInDynamicTable(f1) && ShouldUpdateDynamicTable(f2)) {
    IncrementIndexes(1);
    res = AddMessageWithKey(std::make_pair(GetIndex(f1) + 1, kStaticTableSize + 1));
  } else if (ShouldUpdateDynamicTable(f1) && IsInDynamicTable(f2)) {
    IncrementIndexes(1);
    res = AddMessageWithKey(std::make_pair(kStaticTableSize + 1, GetIndex(f2)));
  } else if (ShouldUpdateDynamicTable(f1) && ShouldUpdateDynamicTable(f2)) {
    IncrementIndexes(2);
    res = AddMessageWithKey(std::make_pair(kStaticTableSize + 2, kStaticTableSize + 1));
  }

  // Increment the dynamic table indexes considering the literal header fields beyond the reserved
  // headers.
  int increment_count = 0;
  for (size_t i = kReservedHeaderCount; i < req_header_fields.size(); ++i) {
    increment_count += ShouldUpdateDynamicTable(req_header_fields[i]);
  }
  // Increment the indexes of all message flows.
  if (increment_count > 0) {
    IncrementIndexes(increment_count);
  }
  return res;
}

size_t GRPCMessageFlow::AddMessageWithKey(std::pair<uint32_t, uint32_t> key) {
  size_t res = 0;
  auto key_iter = message_flows_.find(key);
  if (key_iter == message_flows_.end()) {
    // Create a new list for this unseen method.
    res = next_message_list_index_;
    message_flows_.emplace(key, res);
    ++next_message_list_index_;
  } else {
    // Append to the existing list.
    res = key_iter->second;
  }
  return res;
}

void GRPCMessageFlow::IncrementIndexes(int count) {
  std::map<std::pair<uint32_t, uint32_t>, size_t> copy;
  for (const auto [k, v] : message_flows_) {
    auto k_copy = k;
    // TODO(yzhao): This would fail if the incremented value got discarded, if the inserted
    // entries into dynamic table causes the pointed cache entry being evicted. In that case,
    // a literal field is produced by the encoder, and we need to handle that to reset the
    // indexes.
    k_copy.first += count;
    k_copy.second += count;
    copy[k_copy] = v;
  }
  message_flows_.swap(copy);
}

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
