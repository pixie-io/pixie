#include "src/stirling/types.h"

namespace pl {
namespace stirling {

stirlingpb::Element DataElement::ToProto() const {
  stirlingpb::Element element_proto;
  element_proto.set_name(name_.data());
  element_proto.set_type(type_);
  element_proto.set_ptype(ptype_);
  return element_proto;
}

}  // namespace stirling
}  // namespace pl
