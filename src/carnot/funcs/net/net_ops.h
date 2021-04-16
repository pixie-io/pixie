#pragma once
#include <arpa/inet.h>
#include <netdb.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/funcs/net/dns.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace net {

using ScalarUDF = px::carnot::udf::ScalarUDF;

class NSLookupUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue addr) { return cache_.Lookup(addr); }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Perform a DNS lookup for the value (experimental).")
        .Details("Experimental UDF to perform a DNS lookup for a given value.")
        .Arg("addr", "An IP address")
        .Example("df.hostname = px.nslookup(df.ip_addr)")
        .Returns("The hostname.");
  }

 private:
  internal::DNSCache& cache_ = internal::DNSCache::GetInstance();
};

void RegisterNetOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace px
