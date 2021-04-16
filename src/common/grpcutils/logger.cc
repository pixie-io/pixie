#include <grpcpp/grpcpp.h>

#include <chrono>
#include <string>

#include <absl/strings/str_format.h>
#include "src/common/base/base.h"
#include "src/common/grpcutils/logger.h"

using grpc::experimental::InterceptionHookPoints;
using grpc::experimental::Interceptor;
using grpc::experimental::ServerRpcInfo;

namespace px {

class LoggingInterceptor : public grpc::experimental::Interceptor {
 public:
  explicit LoggingInterceptor(grpc::experimental::ServerRpcInfo* info) : info_(info) {}

  void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
      start_time_ = std::chrono::high_resolution_clock::now();
    }

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::PRE_SEND_STATUS)) {
      status_ = methods->GetSendStatus();
    }

    if (methods->QueryInterceptionHookPoint(InterceptionHookPoints::POST_RECV_CLOSE)) {
      auto stop_time = std::chrono::high_resolution_clock::now();
      auto duration = stop_time - start_time_;
      auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
      if (status_.ok()) {
        LOG(INFO) << absl::StrFormat("%s: %s", info_->method(), PrettyDuration(duration_ns));
      } else {
        LOG(ERROR) << absl::StrFormat("%s: %s %s", info_->method(), PrettyDuration(duration_ns),
                                      status_.error_message());
      }
    }

    methods->Proceed();
  }

 private:
  grpc::experimental::ServerRpcInfo* info_;
  grpc::Status status_;
  std::chrono::high_resolution_clock::time_point start_time_;
};

Interceptor* px::LoggingInterceptorFactory::CreateServerInterceptor(ServerRpcInfo* info) {
  return new LoggingInterceptor(info);
}

}  // namespace px
