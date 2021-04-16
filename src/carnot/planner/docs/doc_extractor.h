#pragma once
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/planner/compiler/compiler.h"
#include "src/carnot/planner/compiler_state/compiler_state.h"
#include "src/carnot/planner/distributed/distributed_plan.h"
#include "src/carnot/planner/distributed/distributed_planner.h"
#include "src/carnot/planner/distributed/tablet_rules.h"
#include "src/carnot/planner/plannerpb/func_args.pb.h"
#include "src/carnot/planner/probes/probes.h"
#include "src/shared/scriptspb/scripts.pb.h"

#include "src/carnot/docspb/docs.pb.h"

namespace px {
namespace carnot {
namespace planner {
namespace docs {

struct DocHolder {
  std::string doc;
  std::string name;
  std::string objtype;
  absl::flat_hash_map<std::string, DocHolder> attrs;

  Status ToProto(docspb::DocstringNode* pb) const;
};

struct DocExtractor {
  DocHolder ExtractDoc(const compiler::QLObjectPtr& qobject);
};

}  // namespace docs
}  // namespace planner
}  // namespace carnot
}  // namespace px
