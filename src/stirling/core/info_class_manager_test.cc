#include <gtest/gtest.h>

#include "src/stirling/core/info_class_manager.h"
#include "src/stirling/source_connectors/seq_gen/seq_gen_connector.h"

namespace pl {
namespace stirling {

using types::DataType;
using types::PatternType;

TEST(InfoClassInfoSchemaTest, infoclass_mgr_proto_getters_test) {
  InfoClassManager info_class_mgr(SeqGenConnector::kSeq0Table);
  auto source = SeqGenConnector::Create("sequences");
  info_class_mgr.SetSourceConnector(source.get(), SeqGenConnector::kSeq0TableNum);

  EXPECT_EQ(SeqGenConnector::kSeq0Table.elements().size(),
            info_class_mgr.Schema().elements().size());
  EXPECT_EQ(SeqGenConnector::kSeq0Table.name(), info_class_mgr.name());

  stirlingpb::InfoClass info_class_pb;
  info_class_pb = info_class_mgr.ToProto();
  EXPECT_EQ(SeqGenConnector::kSeq0Table.elements().size(), info_class_pb.schema().elements_size());
  EXPECT_EQ(SeqGenConnector::kSeq0Table.name(), info_class_pb.schema().name());
  EXPECT_EQ(0, info_class_pb.id());

  info_class_mgr.SetSubscription(true);
  stirlingpb::InfoClass subscribe_pb;
  subscribe_pb = info_class_mgr.ToProto();
  EXPECT_TRUE(subscribe_pb.subscribed());
}

}  // namespace stirling
}  // namespace pl
