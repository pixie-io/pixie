#include <memory>
#include <utility>
#include <vector>

#include "src/common/base/base.h"

#include "src/stirling/data_table.h"
#include "src/stirling/info_class_manager.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

bool InfoClassManager::SamplingRequired() const { return CurrentTime() > NextSamplingTime(); }

bool InfoClassManager::PushRequired() const {
  // Note: It's okay to exercise an early Push, by returning true before the final return,
  // but it is not okay to 'return false' in this function.

  if (data_table_->OccupancyPct() > occupancy_pct_threshold_) {
    return true;
  }

  if (data_table_->Occupancy() > occupancy_threshold_) {
    return true;
  }

  return CurrentTime() > NextPushTime();
}

void InfoClassManager::SampleData(ConnectorContext* ctx) {
  source_->TransferData(ctx, source_table_num_, data_table_);

  // Update the last sampling time.
  last_sampled_ = CurrentTime();

  sampling_count_++;
}

void InfoClassManager::PushData(PushDataCallback agent_callback) {
  auto record_batches = data_table_->ConsumeRecordBatches();
  for (auto& record_batch : record_batches) {
    if (!record_batch.records_uptr->empty()) {
      agent_callback(id(), record_batch.tablet_id, std::move(record_batch.records_uptr));
    }
  }

  // Update the last pushed time.
  last_pushed_ = CurrentTime();

  push_count_++;
}

stirlingpb::InfoClass InfoClassManager::ToProto() const {
  stirlingpb::InfoClass info_class_proto;
  // Populate the proto with Elements.
  for (auto element : schema_.elements()) {
    stirlingpb::Element* element_proto_ptr = info_class_proto.add_elements();
    element_proto_ptr->MergeFrom(element.ToProto());
  }

  // Add all the other fields for the proto.
  info_class_proto.set_name(std::string(schema_.name()));
  info_class_proto.set_id(id_);
  info_class_proto.set_subscribed(subscribed_);
  info_class_proto.set_sampling_period_millis(sampling_period_.count());
  info_class_proto.set_push_period_millis(push_period_.count());

  return info_class_proto;
}

}  // namespace stirling
}  // namespace pl
