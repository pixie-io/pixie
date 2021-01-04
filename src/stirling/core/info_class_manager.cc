#include <memory>
#include <utility>

#include "src/common/base/base.h"

#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/core/source_connector.h"

namespace pl {
namespace stirling {

bool InfoClassManager::SamplingRequired() const {
  return std::chrono::steady_clock::now() > NextSamplingTime();
}

bool InfoClassManager::PushRequired() const {
  // Note: It's okay to exercise an early Push, by returning true before the final return,
  // but it is not okay to 'return false' in this function.

  if (data_table_->OccupancyPct() > occupancy_pct_threshold_) {
    return true;
  }

  if (data_table_->Occupancy() > occupancy_threshold_) {
    return true;
  }

  return std::chrono::steady_clock::now() > NextPushTime();
}

void InfoClassManager::InitContext(ConnectorContext* ctx) { source_->InitContext(ctx); }

void InfoClassManager::SampleData(ConnectorContext* ctx) {
  source_->TransferData(ctx, source_table_num_, data_table_);

  // Update the last sampling time.
  last_sampled_ = std::chrono::steady_clock::now();

  sampling_count_++;
}

void InfoClassManager::PushData(DataPushCallback agent_callback) {
  auto record_batches = data_table_->ConsumeRecords();
  for (auto& record_batch : record_batches) {
    if (!record_batch.records.empty()) {
      Status s = agent_callback(
          id(), record_batch.tablet_id,
          std::make_unique<types::ColumnWrapperRecordBatch>(std::move(record_batch.records)));
      LOG_IF(DFATAL, !s.ok()) << absl::Substitute("Failed to push data. Message = $0", s.msg());
    }
  }

  // Update the last pushed time.
  last_pushed_ = std::chrono::steady_clock::now();

  push_count_++;
}

stirlingpb::InfoClass InfoClassManager::ToProto() const {
  stirlingpb::InfoClass info_class_proto;
  info_class_proto.set_type(type_);
  info_class_proto.mutable_schema()->CopyFrom(schema_.ToProto());
  info_class_proto.set_id(id_);
  info_class_proto.set_subscribed(subscribed_);
  info_class_proto.set_sampling_period_millis(sampling_period_.count());
  info_class_proto.set_push_period_millis(push_period_.count());

  return info_class_proto;
}

}  // namespace stirling
}  // namespace pl
