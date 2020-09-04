#pragma once
#include <arpa/inet.h>
#include <netdb.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace funcs {
namespace net {

using ScalarUDF = pl::carnot::udf::ScalarUDF;

namespace internal {

inline std::string Lookup(std::string addr) {
  struct sockaddr_in sa;

  constexpr size_t kMaxHostnameSize = 512;
  char node[kMaxHostnameSize];

  memset(&sa, 0, sizeof sa);
  sa.sin_family = AF_INET;

  inet_pton(AF_INET, addr.c_str(), &sa.sin_addr);

  int res =
      getnameinfo((struct sockaddr*)&sa, sizeof(sa), node, sizeof(node), NULL, 0, NI_NAMEREQD);

  if (res) {
    return gai_strerror(res);
  }
  return node;
}

}  // namespace internal

class NSLookupUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue addr) { return internal::Lookup(addr); }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Perform a DNS lookup for the value (experimental).")
        .Details("Experimental UDF to perfom a DNS lookup for a given value..")
        .Arg("addr", "A IP address")
        .Example("df.hostname = px.nslookup(df.ip_addr)")
        .Returns("The hostname.");
  }
};

void RegisterNetOpsOrDie(pl::carnot::udf::Registry* registry);

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace pl
