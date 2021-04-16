#include "src/carnot/planner/docs/doc_extractor.h"

namespace px {
namespace carnot {
namespace planner {
namespace docs {

Status DocHolder::ToProto(docspb::DocstringNode* pb) const {
  pb->set_name(name);
  pb->set_docstring(doc);
  for (const auto& [k, v] : attrs) {
    PL_UNUSED(k);
    PL_RETURN_IF_ERROR(v.ToProto(pb->add_children()));
  }
  return Status::OK();
}

DocHolder DocExtractor::ExtractDoc(const compiler::QLObjectPtr& qobject) {
  DocHolder doc;
  doc.name = qobject->name();
  doc.doc = qobject->doc_string();
  for (const auto& [k, v] : qobject->attributes()) {
    doc.attrs[k] = ExtractDoc(v);
  }

  for (const auto& [k, v] : qobject->methods()) {
    doc.attrs[k] = ExtractDoc(v);
  }
  return doc;
}

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace px
