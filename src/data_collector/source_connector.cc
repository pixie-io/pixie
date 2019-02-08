#include "src/data_collector/source_connector.h"
#include <glog/logging.h>
#include "src/common/macros.h"

namespace pl {
namespace datacollector {

//------------------------------------------------------
// SourceConnector (Base Class)
//------------------------------------------------------

// Get the data from the collector, but also keep track of some basic stats.
RawDataBuf SourceConnector::GetData() { return GetDataImpl(); }

//------------------------------------------------------
// EBPFConnector
//------------------------------------------------------

// Placeholder for an EBPFConnector.
// Change as needed.
EBPFConnector::EBPFConnector(const std::string& name, const std::string& bpf_program,
                             const std::string& kernel_event, const std::string& fn_name)
    : SourceConnector(name) {
  PL_UNUSED(bpf_program);
  PL_UNUSED(kernel_event);
  PL_UNUSED(fn_name);

  // FIXME(oazizi): Placeholder to avoid seg-fault for now.
  uint32_t record_size = sizeof(uint64_t) + sizeof(double) + sizeof(uint32_t);
  data_buf_.resize(1000 * record_size);
}

Status EBPFConnector::PopulateSchema(InfoClassSchema* schema) {
  CHECK_EQ(schema->NumElements(), 0ULL);
  PL_RETURN_IF_ERROR(schema->AddElement("f0", DataType::INT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));
  PL_RETURN_IF_ERROR(schema->AddElement("f1", DataType::FLOAT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));
  PL_RETURN_IF_ERROR(schema->AddElement("f2", DataType::INT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));

  return Status::OK();
}

// Placeholder for an EBPFConnector.
// Change as needed.
RawDataBuf EBPFConnector::GetDataImpl() {
  uint64_t num_records = 1;
  return RawDataBuf(num_records, data_buf_.data());
}

//------------------------------------------------------
// OpenTracingConnector
//------------------------------------------------------

// Placeholder for an OpenTracingConnector.
// Change as needed.
OpenTracingConnector::OpenTracingConnector(const std::string& name) : SourceConnector(name) {
  // FIXME(oazizi): Placeholder to avoid seg-fault for now.
  uint32_t record_size = sizeof(double) + sizeof(double) + sizeof(float) + sizeof(float);
  data_buf_.resize(1000 * record_size);
}

Status OpenTracingConnector::PopulateSchema(InfoClassSchema* schema) {
  CHECK_EQ(schema->NumElements(), 0ULL);
  PL_RETURN_IF_ERROR(schema->AddElement("f0", DataType::FLOAT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));
  PL_RETURN_IF_ERROR(schema->AddElement("f1", DataType::INT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));
  PL_RETURN_IF_ERROR(schema->AddElement("f2", DataType::FLOAT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));
  PL_RETURN_IF_ERROR(schema->AddElement("f3", DataType::INT64,
                                        Element_State::Element_State_COLLECTED_NOT_SUBSCRIBED));

  return Status::OK();
}

// Placeholder for an OpenTracingConnector.
// Change as needed.
RawDataBuf OpenTracingConnector::GetDataImpl() {
  uint32_t num_records = 1;
  return RawDataBuf(num_records, data_buf_.data());
}

}  // namespace datacollector
}  // namespace pl
