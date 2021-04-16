#include <memory>
#include <utility>

#include "src/common/base/base.h"

#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/core/source_connector.h"

namespace px {
namespace stirling {

bool InfoClassManager::SamplingRequired() const { return sample_push_freq_mgr_.SamplingRequired(); }

bool InfoClassManager::PushRequired() const {
  return sample_push_freq_mgr_.PushRequired(data_table_->OccupancyPct(), data_table_->Occupancy());
}

void InfoClassManager::InitContext(ConnectorContext* ctx) { source_->InitContext(ctx); }

void InfoClassManager::SampleData(ConnectorContext* ctx) {
  source_->TransferData(ctx, source_table_num_, data_table_);
  sample_push_freq_mgr_.Sample();
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
  sample_push_freq_mgr_.Push();
}

stirlingpb::InfoClass InfoClassManager::ToProto() const {
  stirlingpb::InfoClass info_class_proto;
  info_class_proto.set_type(type_);
  info_class_proto.mutable_schema()->CopyFrom(schema_.ToProto());
  info_class_proto.set_id(id_);
  info_class_proto.set_subscribed(subscribed_);
  info_class_proto.set_sampling_period_millis(sample_push_freq_mgr_.sampling_period().count());
  info_class_proto.set_push_period_millis(sample_push_freq_mgr_.push_period().count());

  return info_class_proto;
}

}  // namespace stirling
}  // namespace px
