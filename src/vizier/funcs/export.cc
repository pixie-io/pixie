#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"
#include "src/vizier/funcs/funcs.h"

DEFINE_string(out_file_path, gflags::StringFromEnv("PL_FUNCS_OUT_FILE", "funcs.pb"),
              "The file to save the serialized UDFInfo");

int main(int argc, char** argv) {
  px::EnvironmentGuard env_guard(&argc, argv);
  px::carnot::udf::Registry registry("registry");
  px::vizier::funcs::VizierFuncFactoryContext ctx;
  RegisterFuncsOrDie(ctx, &registry);

  // Serialzie the registry as a protobuf
  std::ofstream out_udf_info;
  out_udf_info.open(FLAGS_out_file_path);

  px::carnot::udfspb::UDFInfo udf_info = registry.ToProto();
  udf_info.SerializeToOstream(&out_udf_info);

  out_udf_info.close();

  return 0;
}
