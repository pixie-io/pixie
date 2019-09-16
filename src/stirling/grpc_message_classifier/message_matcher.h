#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/base/status.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/stirling/grpc_message_classifier/trial_parser.h"
#include "src/stirling/http2.h"

namespace pl {
namespace stirling {
namespace grpc {

// TODO(oazizi): If we merge this into Stirling, we have a similar object in event_parser.h.
//               Consider refactoring that into a shared location.
enum class MessageDirection {
  kUnknown,
  kRequest,
  kResponse,
};

/**
 * The MessageGroupTypeClassifier class attempts to find the request and response message
 * types of a sequence of related messages to an RPC whose message types are not known.
 *
 * The MessageGroupTypeClassifier initially starts with a candidate set of all possible RPCs,
 * as extracted from a service descriptor database. Then each time a message is
 * observed, it prunes out the possible set of RPCs that could match.
 *
 * After presenting one or more messages, the MessageGroupTypeClassifier can be queried
 * to see what the current set of potential message types is for the sequence.
 */
class MessageGroupTypeClassifier {
 public:
  /**
   * Initializes a message group type classifier, using the message types and RPC calls from
   * the service  descriptor database.
   *
   * @param desc_db Service descriptor database.
   * @param service_names List of services names, used for filtering initial candidate set.
   * @param msg_parse_opts Options to control parsing rules.
   */
  MessageGroupTypeClassifier(::pl::grpc::ServiceDescriptorDatabase* desc_db,
                             std::vector<std::string_view> service_names,
                             ParseAsOpts msg_parse_opts);

  /**
   * Analyzes the message to filter out the set of possible RPCs and their matching message types.
   *
   * @param message The message being analyzed for matching RPCs.
   * @param direction The message direction (request or response), if known. This restricts the
   * candidate set of RPCs.
   */
  void AddMessage(const std::string& message,
                  MessageDirection direction = MessageDirection::kUnknown, std::string label = "");

  /**
   * Return the current set of candidate RPCs.
   *
   * @return set of RPC req-resp message types.
   */
  const std::set<::google::protobuf::MethodDescriptorProto>& MatchingRPCs() const {
    return matching_rpcs_;
  }

  /**
   * Return the set of message types from the candidate RPCs.
   *
   * @param direction Whether to include message types from the requests, responses or both.
   * @return Set of candidate message types.
   */
  std::set<std::string> MatchingMessageTypes(
      MessageDirection direction = MessageDirection::kUnknown) const;

  /**
   * Return the history of how the candidate set size has progressed.
   * Element 0 has the initial candidate set size, followed by additional elements
   * for each call to AddMessage().
   *
   * @return vector of candidate set sizes.
   */
  const std::vector<size_t>& num_candidates_history() const { return num_candidates_history_; }

  /**
   * Return the number of messages that have been added to this MessageMatcher via AddMessage().
   *
   * @param direction Specify whether to return the number of requests (MessageDirection::kRequest),
   *                  responses (MessageDirection::kResponse) or either
   * (MessageDirection::kUnknown).
   * @return message count.
   */
  int NumMessages(MessageDirection direction) const;

  std::set<std::string>& labels() { return labels_; }

 private:
  /**
   * Uses the service descriptor database to check whether a message can be parsed
   * as the given message type.
   *
   * @param message_type Message type candidate.
   * @param message Message being checked.
   * @return true if message is compatible with the message type.
   */
  bool IsParseable(const std::string& message_type, const std::string& message);

  /**
   * Helper function that takes a set of candidate message type pairs, and prunes that set based
   * on an additional message.
   *
   * @param message_type_pairs: The candidate set of message type pairs.
   * @param message: The request or response message being analyzed for matching message type pairs.
   * @param direction: Whether message is a request or response.
   * @return A pruned set of potentially matching message type pairs.
   */
  std::set<::google::protobuf::MethodDescriptorProto> FindMatchingRPCs(
      const std::set<::google::protobuf::MethodDescriptorProto>& rpcs, const std::string& message,
      MessageDirection direction);

  /**
   * Generates the initial set of candidate RPCs, from the descriptor database.
   *
   * @param service_names: A list of service names from which to select the initial set of candidate
   * RPCs.
   */
  void InitializeCandidateRPCs(std::vector<std::string_view> service_names);

  // Pointer to database of all services and message types being considered.
  ::pl::grpc::ServiceDescriptorDatabase* desc_db_;

  // The set of matching message types so far. This set should only stay the same
  // or get smaller with calls to AddMessage().
  std::set<::google::protobuf::MethodDescriptorProto> matching_rpcs_;

  // Configuration options to control the behavior of the protobuf parser.
  grpc::ParseAsOpts msg_parse_opts_;

  // History of the number of candidates.
  // TODO(oazizi): If this class is ever used in production code, remove or disable this.
  std::vector<size_t> num_candidates_history_;

  // Number of messages added.
  int req_message_count = 0;
  int resp_message_count = 0;

  std::set<std::string> labels_;
};

/**
 * @brief Given a list of gRPC request & response pairs, and the encoded headers from the requests,
 * sort them into groups for different gRPC methods. The resultant groups can then be used to feed
 * into MessageMatcher as multiple request & response pairs of a single gRPC method.
 */
class GRPCMessageFlow {
 public:
  struct GRPCMethodMessageList {
    std::vector<std::string_view> reqs;
    std::vector<std::string_view> resps;
  };

  /**
   * @brief Add a pair request and response, and the request's encoded header fields. The message
   * pairs of individual RPC method is inferred from the encoded header fields, and put into the
   * corresponding lists.
   *
   * @return The index to the message list that accepts the input messages, or a error status.
   */
  StatusOr<size_t> AddMessage(
      const std::vector<::pl::stirling::http2::HeaderField>& req_header_fields);

 private:
  FRIEND_TEST(GRPCMessageFlowTest, ReceivesIndexedHeaderFields);

  // Returns the index to the message list that accepts the input messages.
  size_t AddMessageWithKey(std::pair<uint32_t, uint32_t> key);

  void IncrementIndexes(int count);

  // The index for the next message list that has not seen before.
  size_t next_message_list_index_ = 0;

  // Key: The first 2 non-static table indexes of the message.
  // Value: Index to the above list.
  // The corresponding resp will be the index+1.
  // TODO(yzhao): The value field can have a decoded path, if we are able to track that.
  std::map<std::pair<uint32_t, uint32_t>, size_t> message_flows_;
};

}  // namespace grpc
}  // namespace stirling
}  // namespace pl
