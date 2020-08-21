#include "src/carnot/planner/docs/doc_extractor.h"

#include <fstream>

namespace pl {
namespace carnot {
namespace planner {
namespace docs {

StatusOr<docspb::InternalPXLDocs> ExtractDocs() {
  IR ir;
  compiler::MutationsIR dynamic_trace;
  RegistryInfo registry_info;
  CompilerState compiler_state(std::make_unique<RelationMap>(), &registry_info, 10, "");
  compiler::ModuleHandler module_handler;

  PL_ASSIGN_OR_RETURN(auto ast_visitor, compiler::ASTVisitorImpl::Create(
                                            &ir, &dynamic_trace, &compiler_state, &module_handler));

  DocExtractor extractor;
  docspb::InternalPXLDocs parent;
  for (const auto& [modname, module] : module_handler) {
    auto mod_doc = extractor.ExtractDoc(module);
    PL_RETURN_IF_ERROR(mod_doc.ToProto(parent.add_docstring_nodes()));
  }
  return parent;
}

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace pl

DEFINE_string(output_file, "input.pb.txt", "File to write AllDocs to.");
int main(int argc, char** argv) {
  pl::EnvironmentGuard env_guard(&argc, argv);
  auto docs_or_s = pl::carnot::planner::docs::ExtractDocs();
  if (!docs_or_s.ok()) {
    LOG(ERROR) << docs_or_s.status().ToString();
    return 1;
  }
  auto docs = docs_or_s.ConsumeValueOrDie();

  std::ofstream output_file;
  output_file.open(FLAGS_output_file);
  output_file << docs.DebugString();
  output_file.close();
}
