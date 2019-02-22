#include <gtest/gtest.h>

#include "src/stirling/bcc_connector.h"
#include "src/stirling/info_class_schema.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

using stirlingpb::Element_State;
using types::DataType;

TEST(InfoClassElementTest, basic_test) {
  InfoClassElement element("user_percentage", DataType::FLOAT64,
                           Element_State::Element_State_NOT_SUBSCRIBED);

  EXPECT_EQ("user_percentage", element.name());
  EXPECT_EQ(DataType::FLOAT64, element.type());
  EXPECT_EQ(Element_State::Element_State_NOT_SUBSCRIBED, element.state());

  stirlingpb::Element element_pb;
  element_pb = element.ToProto();
  EXPECT_EQ("user_percentage", element_pb.name());
  EXPECT_EQ(DataType::FLOAT64, element_pb.type());
  EXPECT_EQ(Element_State::Element_State_NOT_SUBSCRIBED, element_pb.state());
}

TEST(InfoClassInfoSchemaTest, basic_test) {
  InfoClassSchema elements = {};
  auto source = BCCCPUMetricsConnector::Create();
  InfoClassManager info_class_mgr("cpu_usage");
  info_class_mgr.SetSourceConnector(source.get());
  InfoClassElement element_1("user_percentage", DataType::FLOAT64,
                             Element_State::Element_State_NOT_SUBSCRIBED);
  InfoClassElement element_2("system_percentage", DataType::FLOAT64,
                             Element_State::Element_State_NOT_SUBSCRIBED);
  InfoClassElement element_3("io_percentage", DataType::FLOAT64,
                             Element_State::Element_State_NOT_SUBSCRIBED);

  info_class_mgr.Schema().push_back(element_1);
  info_class_mgr.Schema().push_back(element_2);
  info_class_mgr.Schema().push_back(element_3);

  EXPECT_EQ(3, info_class_mgr.Schema().size());
  EXPECT_EQ("cpu_usage", info_class_mgr.name());
  EXPECT_EQ("ebpf_cpu_metrics", source->source_name());

  stirlingpb::InfoClass info_class_pb;
  info_class_pb = info_class_mgr.ToProto();
  EXPECT_EQ(3, info_class_pb.elements_size());
  EXPECT_EQ("cpu_usage", info_class_pb.name());
  EXPECT_EQ("user_percentage", info_class_pb.elements(0).name());
  EXPECT_EQ(DataType::FLOAT64, info_class_pb.elements(0).type());
  EXPECT_EQ(0, info_class_pb.id());
  auto metadata = info_class_pb.metadata();
  EXPECT_EQ("ebpf_cpu_metrics", metadata["source"]);

  info_class_mgr.UpdateElementSubscription(0, Element_State::Element_State_SUBSCRIBED);
  EXPECT_EQ(Element_State::Element_State_SUBSCRIBED, info_class_mgr.GetElement(0).state());

  stirlingpb::InfoClass subscribe_pb;
  subscribe_pb = info_class_mgr.ToProto();
  EXPECT_EQ(Element_State::Element_State_SUBSCRIBED, subscribe_pb.elements(0).state());
}

}  // namespace stirling
}  // namespace pl
