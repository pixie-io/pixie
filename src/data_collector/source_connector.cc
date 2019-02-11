#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

Status SourceConnector::PopulateSchema(InfoClassSchema* schema) {
  for (auto element : elements_) {
    schema->AddElement(element);
  }
  return Status::OK();
}

// Placeholder for an EBPFConnector.
// Change as needed.
RawDataBuf EBPFConnector::GetDataImpl() {
  uint64_t num_records = 1;
  return RawDataBuf(num_records, data_buf_.data());
}

// Placeholder for an OpenTracingConnector.
// Change as needed.
RawDataBuf OpenTracingConnector::GetDataImpl() {
  uint32_t num_records = 1;
  return RawDataBuf(num_records, data_buf_.data());
}

}  // namespace datacollector
}  // namespace pl
