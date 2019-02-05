#include "src/data_collector/source_connector.h"
#include <glog/logging.h>
#include "src/common/macros.h"

namespace pl {
namespace datacollector {

//------------------------------------------------------
// SourceConnector (Base Class)
//------------------------------------------------------

// Get the data from the collector, but also keep track of some basic stats.
void* SourceConnector::GetData() { return GetDataImpl(); }

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
  data_buf_ = malloc(10 * (sizeof(uint64_t) + sizeof(double) + sizeof(uint32_t)));
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
EBPFConnector::~EBPFConnector() { free(data_buf_); }

// Placeholder for an EBPFConnector.
// Change as needed.
void* EBPFConnector::GetDataImpl() { return data_buf_; }

//------------------------------------------------------
// OpenTracingConnector
//------------------------------------------------------

// Placeholder for an OpenTracingConnector.
// Change as needed.
OpenTracingConnector::OpenTracingConnector(const std::string& name) : SourceConnector(name) {
  data_buf_ = malloc(10 * (sizeof(double) + sizeof(double) + sizeof(float) + sizeof(float)));
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
OpenTracingConnector::~OpenTracingConnector() { free(data_buf_); }

// Placeholder for an OpenTracingConnector.
// Change as needed.
void* OpenTracingConnector::GetDataImpl() { return data_buf_; }

}  // namespace datacollector
}  // namespace pl
