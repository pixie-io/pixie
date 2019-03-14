#include <grpcpp/grpcpp.h>

#include <condition_variable>
#include <string>
#include <vector>

#include <sole.hpp>

#include "src/common/common.h"
#include "src/common/grpc_utils/logger.h"
#include "src/vizier/controller/service.h"

// GRPC headers have lots of unused variable warnings.
PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/proto/service.grpc.pb.h"
PL_SUPPRESS_WARNINGS_END()

DEFINE_int32(vizier_port, gflags::Int32FromEnv("PL_VIZIER_PORT", 40000),
             "The port to run vizier on.");

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);

  std::string addr = "0.0.0.0:";
  addr += std::to_string(FLAGS_vizier_port);
  LOG(INFO) << "Vizier";

  grpc::ServerBuilder builder;
  pl::vizier::VizierServiceImpl srvc;
  builder.AddListeningPort(addr, grpc::InsecureServerCredentials());
  std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> creators;
  creators.push_back(std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>(
      new pl::LoggingInterceptorFactory()));
  builder.experimental().SetInterceptorCreators(std::move(creators));
  builder.RegisterService(&srvc);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  LOG(INFO) << "Vizier listening on " << addr << std::endl;
  // TODO(zasgar): Add ctrl-c intercept handlers for graceful cleanup/shutdown.
  server->Wait();

  pl::ShutdownEnvironmentOrDie();
}
