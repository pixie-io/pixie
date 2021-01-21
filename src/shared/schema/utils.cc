#include <vector>

#include "src/shared/schema/utils.h"

namespace pl {
namespace {

table_store::schema::Relation InfoClassProtoToRelation(
    const stirling::stirlingpb::InfoClass& info_class_pb) {
  table_store::schema::Relation relation;
  for (const auto& element : info_class_pb.schema().elements()) {
    relation.AddColumn(element.type(), element.name(), element.stype(), element.desc());
  }
  return relation;
}

}  // namespace

RelationInfo ConvertInfoClassPBToRelationInfo(
    const stirling::stirlingpb::InfoClass& info_class_pb) {
  if (info_class_pb.schema().tabletized()) {
    return RelationInfo(info_class_pb.schema().name(), info_class_pb.id(),
                        info_class_pb.schema().desc(), info_class_pb.schema().tabletization_key(),
                        InfoClassProtoToRelation(info_class_pb));
  }
  return RelationInfo(info_class_pb.schema().name(), info_class_pb.id(),
                      info_class_pb.schema().desc(), InfoClassProtoToRelation(info_class_pb));
}

std::vector<RelationInfo> ConvertSubscribePBToRelationInfo(
    const stirling::stirlingpb::Subscribe& subscribe_pb) {
  std::vector<RelationInfo> relation_info_vec;
  relation_info_vec.reserve(subscribe_pb.subscribed_info_classes_size());
  for (const auto& info_class : subscribe_pb.subscribed_info_classes()) {
    relation_info_vec.emplace_back(ConvertInfoClassPBToRelationInfo(info_class));
  }
  return relation_info_vec;
}

}  // namespace pl
