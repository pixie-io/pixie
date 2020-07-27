#pragma once
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/proto/stirling.pb.h"
#include "src/table_store/schema/relation.h"

namespace pl {

/**
 * A relation and accompanying information such as names and ids.
 */
struct RelationInfo {
  RelationInfo() {}
  RelationInfo(std::string name, uint64_t id, table_store::schema::Relation relation)
      : name(std::move(name)), id(id), tabletized(false), relation(std::move(relation)) {}

  RelationInfo(std::string name, uint64_t id, uint64_t tabletization_key_idx,
               table_store::schema::Relation relation)
      : name(std::move(name)),
        id(id),
        tabletized(true),
        tabletization_key_idx(tabletization_key_idx),
        relation(std::move(relation)) {}

  std::string name;
  uint64_t id;
  bool tabletized;
  uint64_t tabletization_key_idx;
  table_store::schema::Relation relation;
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
