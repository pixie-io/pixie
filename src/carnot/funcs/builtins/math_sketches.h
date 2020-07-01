#pragma once
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "src/carnot/udf/registry.h"
#include "src/shared/types/types.h"
#include "third_party/tdigest/tdigest.h"

namespace pl {
namespace carnot {
namespace builtins {

// TODO(zasgar): PL-419 Replace this when we add support for structs.
template <typename TArg>
class QuantilesUDA : public udf::UDA {
 public:
  QuantilesUDA() : digest_(1000) {}
  void Update(FunctionContext*, TArg val) { digest_.add(val.val); }
  void Merge(FunctionContext*, const QuantilesUDA& other) { digest_.merge(&other.digest_); }

  StringValue Finalize(FunctionContext*) {
    rapidjson::Document d;
    d.SetObject();
    d.AddMember("p01", digest_.quantile(0.01), d.GetAllocator());
    d.AddMember("p10", digest_.quantile(0.10), d.GetAllocator());
    d.AddMember("p50", digest_.quantile(0.50), d.GetAllocator());
    d.AddMember("p90", digest_.quantile(0.90), d.GetAllocator());
    d.AddMember("p99", digest_.quantile(0.99), d.GetAllocator());
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);
    return sb.GetString();
  }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::ExplicitRule::Create<QuantilesUDA>(types::ST_QUANTILES, {types::ST_NONE})};
  }

 protected:
  tdigest::TDigest digest_;
};

void RegisterMathSketchesOrDie(udf::Registry* registry);

}  // namespace builtins
}  // namespace carnot
}  // namespace pl
