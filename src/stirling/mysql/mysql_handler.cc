#include "src/stirling/mysql/mysql_handler.h"
#include <memory>
#include "src/stirling/mysql/mysql.h"

namespace pl {
namespace stirling {
namespace mysql {

// TODO(chengruizhe): Implement in the next diff.
StatusOr<std::unique_ptr<ErrResponse>> HandleErrMessage(std::deque<Packet>* resp_packets) {
  PL_UNUSED(resp_packets);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

StatusOr<std::unique_ptr<OKResponse>> HandleOKMessage(std::deque<Packet>* resp_packets) {
  PL_UNUSED(resp_packets);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

StatusOr<std::unique_ptr<Resultset>> HandleResultset(std::deque<Packet>* resp_packets) {
  PL_UNUSED(resp_packets);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

StatusOr<std::unique_ptr<StmtPrepareOKResponse>> HandleStmtPrepareOKResponse(
    std::deque<Packet>* resp_packets, std::map<int, ReqRespEvent>* prepare_map) {
  PL_UNUSED(resp_packets);
  PL_UNUSED(prepare_map);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

StatusOr<std::unique_ptr<StmtExecuteRequest>> HandleStmtExecuteRequest(
    const Packet& req_packet, std::map<int, ReqRespEvent>* prepare_map) {
  PL_UNUSED(req_packet);
  PL_UNUSED(prepare_map);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

StatusOr<std::unique_ptr<StringRequest>> HandleStringRequest(const Packet& req_packet) {
  PL_UNUSED(req_packet);
  return error::Unimplemented("Placeholder. Will be implemented soon.");
}

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
