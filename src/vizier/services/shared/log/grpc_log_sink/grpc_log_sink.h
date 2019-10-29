#pragma once

#include <glog/logging.h>
#include <grpcpp/grpcpp.h>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "src/common/base/macros.h"

PL_SUPPRESS_WARNINGS_START()
#include "src/vizier/services/cloud_connector/cloud_connectorpb/service.grpc.pb.h"
// TODO(nserrino/michelle): Fix this lint issue so that we can remove the warning.
// NOLINTNEXTLINE(build/include_subdir)
#include "blockingconcurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

namespace pl {
namespace vizier {
namespace services {
namespace shared {
namespace log {

class GRPCLogSink : public google::LogSink {
 public:
  GRPCLogSink(std::unique_ptr<cloud_connector::CloudConnectorService::StubInterface> stub,
              const std::string_view& pod, const std::string_view& svc,
              const int64_t max_queue_size, const std::chrono::milliseconds max_wait_time_ms)
      : max_wait_time_ms_(max_wait_time_ms),
        max_queue_size_(max_queue_size),
        pod_(pod),
        svc_(svc),
        stub_(std::move(stub)) {
    request_writer_ = stub_->TransferLog(&context_, &response_);
    network_thread_ = std::thread([this] { this->PeriodicallyTransferLogs(); });
  }

  static std::unique_ptr<GRPCLogSink> CreateGRPCLogSink(
      const std::string_view& remote_addr, std::shared_ptr<grpc::ChannelCredentials> creds,
      const std::string_view& pod, const std::string_view& svc) {
    std::unique_ptr<cloud_connector::CloudConnectorService::StubInterface> stub =
        cloud_connector::CloudConnectorService::NewStub(
            grpc::CreateChannel(remote_addr.data(), creds));
    return std::make_unique<GRPCLogSink>(
        std::move(stub), pod, svc, 10 /* max queue size before flush */,
        std::chrono::milliseconds(10 * 1000) /* max time between flushes */);
  }

  virtual ~GRPCLogSink() {
    request_writer_->WritesDone();
    grpc::Status status = request_writer_->Finish();
    if (!status.ok()) {
      perror("Could not close TransferLog writer.");
    }
    TerminateNetworkThread();
  }

  void PeriodicallyTransferLogs() {
    while (run_network_thread_) {
      std::mutex mux;
      std::unique_lock<std::mutex> lock(mux);
      std::cv_status s = flush_queue_cv_.wait_for(lock, max_wait_time_ms_);
      // Ignore spurious wakeups
      if (s == std::cv_status::timeout ||
          static_cast<int64_t>(log_queue_.size_approx()) >= max_queue_size_) {
        FlushLogQueue();
      }
    }
  }

  void FlushLogQueue() {
    std::lock_guard<std::mutex> lock(flush_mutex_);

    if (log_queue_.size_approx() == 0) {
      return;
    }

    cloud_connector::TransferLogRequest req;

    std::vector<cloud_connector::LogMessage> msgs(max_queue_size_);
    auto dequeued = log_queue_.try_dequeue_bulk(&msgs[0], log_queue_.size_approx());
    for (size_t i = 0; i < dequeued; ++i) {
      auto logptr = req.add_batched_logs();
      *logptr = msgs[i];
    }

    bool success = request_writer_->Write(req);
    if (!success) {
      perror("Error: Could not write logs to TransferLog stream because it is closed");
    }
  }

  void TerminateNetworkThread() {
    if (run_network_thread_) {
      run_network_thread_ = false;
      network_thread_.join();
    }
  }

  virtual void send(google::LogSeverity severity, const char* /* full_filename */,
                    const char* base_filename, int line, const struct tm* tm_time,
                    const char* message, size_t message_len) {
    cloud_connector::LogMessage msg;
    const std::string log = ToString(severity, base_filename, line, tm_time, message, message_len);
    msg.set_pod(pod_.data());
    msg.set_svc(svc_.data());
    msg.set_log(log);

    if (severity == google::GLOG_FATAL) {
      TerminateNetworkThread();
    }

    bool success = log_queue_.enqueue(msg);
    if (!success) {
      perror("GRPC Log Sink failed to enqueue message.");
    }

    if (severity == google::GLOG_FATAL) {
      FlushLogQueue();
    } else if (static_cast<int64_t>(log_queue_.size_approx()) >= max_queue_size_) {
      flush_queue_cv_.notify_one();
    }
  }

 private:
  std::mutex flush_mutex_;
  std::condition_variable flush_queue_cv_;
  std::atomic<bool> run_network_thread_ = true;

  const std::chrono::milliseconds max_wait_time_ms_;
  const int64_t max_queue_size_;

  const std::string_view pod_;
  const std::string_view svc_;

  grpc::ClientContext context_;
  cloud_connector::TransferLogResponse response_;
  std::unique_ptr<cloud_connector::CloudConnectorService::StubInterface> stub_;
  std::unique_ptr<grpc::ClientWriterInterface<cloud_connector::TransferLogRequest>> request_writer_;

  moodycamel::BlockingConcurrentQueue<cloud_connector::LogMessage> log_queue_;
  std::thread network_thread_;
};

}  // namespace log
}  // namespace shared
}  // namespace services
}  // namespace vizier
}  // namespace pl
