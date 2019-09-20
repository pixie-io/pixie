#ifdef __linux__
#include <cstring>
#include <ctime>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

Status SourceConnector::Init() {
  if (state_ != State::kUninitialized) {
    return error::Internal("Cannot re-initialize a connector [current state = $0].",
                           StateName(state_));
  }
  Status s = InitImpl();
  state_ = s.ok() ? State::kActive : State::kErrors;
  return s;
}

void SourceConnector::TransferData(ConnectorContext* ctx, uint32_t table_num,
                                   DataTable* data_table) {
  DCHECK(ctx != nullptr);
  DCHECK_LT(table_num, num_tables())
      << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
  return TransferDataImpl(ctx, table_num, data_table);
}

Status SourceConnector::Stop() {
  if (state_ != State::kActive) {
    return error::Internal("Cannot stop connector that is not active [current state = $0].",
                           StateName(state_));
  }
  Status s = StopImpl();
  state_ = s.ok() ? State::kStopped : State::kErrors;
  return s;
}

}  // namespace stirling
}  // namespace pl

#endif
