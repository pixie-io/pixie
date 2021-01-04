#ifdef __linux__
#include <cstring>
#include <ctime>

#include <magic_enum.hpp>

#include "src/stirling/core/source_connector.h"

namespace pl {
namespace stirling {

Status SourceConnector::Init() {
  if (state_ != State::kUninitialized) {
    return error::Internal("Cannot re-initialize a connector [current state = $0].",
                           magic_enum::enum_name(static_cast<State>(state_)));
  }
  Status s = InitImpl();
  state_ = s.ok() ? State::kActive : State::kErrors;
  return s;
}

void SourceConnector::InitContext(ConnectorContext* ctx) { InitContextImpl(ctx); }

void SourceConnector::TransferData(ConnectorContext* ctx, uint32_t table_num,
                                   DataTable* data_table) {
  DCHECK(ctx != nullptr);
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
  TransferDataImpl(ctx, table_num, data_table);
}

Status SourceConnector::Stop() {
  if (state_ != State::kActive) {
    return Status::OK();
  }

  // Update state first, so that StopImpl() can act accordingly.
  // For example, SocketTraceConnector::AttachHTTP2probesLoop() exists loop when state_ is
  // kStopped; and SocketTraceConnector::StopImpl() joins the thread.
  state_ = State::kStopped;
  Status s = StopImpl();
  if (!s.ok()) {
    state_ = State::kErrors;
  }
  return s;
}

}  // namespace stirling
}  // namespace pl

#endif
