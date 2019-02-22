#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

using stirlingpb::Element_State;
using types::DataType;

/**
 * @brief This is a test source connector to be used for testing.
 * WARNING: Any changes to the schema may break existing unit tests.
 *
 */
class TestSourceConnector : public SourceConnector {
 public:
  static std::unique_ptr<SourceConnector> Create() {
    InfoClassSchema elements = {
        InfoClassElement("field_0", DataType::TIME64NS,
                         Element_State::Element_State_NOT_SUBSCRIBED),
        InfoClassElement("field_1", DataType::FLOAT64, Element_State::Element_State_NOT_SUBSCRIBED),
        InfoClassElement("field_2", DataType::FLOAT64, Element_State::Element_State_NOT_SUBSCRIBED),
        InfoClassElement("field_3", DataType::FLOAT64,
                         Element_State::Element_State_NOT_SUBSCRIBED)};
    return std::unique_ptr<SourceConnector>(new TestSourceConnector("dummy_connector", elements));
  }

  Status InitImpl() override { return Status::OK(); }

  Status StopImpl() override { return Status::OK(); }

  // TODO(kgandhi): Add meaningful predictable data that can be returned and tested.
  RawDataBuf GetDataImpl() override {
    uint64_t num_records = 1;
    return RawDataBuf(num_records, data_buf_.data());
  }

 protected:
  explicit TestSourceConnector(const std::string& name, const InfoClassSchema& elements)
      : SourceConnector(SourceType::kUnknown, name, elements) {}

 private:
  std::vector<uint8_t> data_buf_;
};

}  // namespace stirling
}  // namespace pl
