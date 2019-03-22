#pragma once
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/schema/relation.h"
#include "src/stirling/proto/collector_config.pb.h"

namespace pl {

/**
 * A relation and accompanying information such as names and ids.
 */
struct RelationInfo {
  RelationInfo(std::string name, uint64_t id, carnot::schema::Relation relation)
      : name(std::move(name)), id(id), relation(std::move(relation)) {}
  std::string name;
  uint64_t id;
  carnot::schema::Relation relation;
};

/**
 * Converts info class proto to a RelationInfo.
 * @param info_class_pb The info class proto.
 * @return RelationInfo.
 */
RelationInfo ConvertInfoClassPBToRelationInfo(const stirling::stirlingpb::InfoClass& info_class_pb);

/**
 * Converts a subscription proto to relation info vector.
 * @param subscribe_pb The subscription proto.
 * @return Relation vector.
 */
std::vector<RelationInfo> ConvertSubscribePBToRelationInfo(
    const stirling::stirlingpb::Subscribe& subscribe_pb);

}  // namespace pl
