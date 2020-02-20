#pragma once

#include <unistd.h>

#include <string>

#include "src/common/base/base.h"
#include "src/common/base/test_utils.h"
#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"

namespace pl {

// DummyTestContainer runs a dummy container. It is meant for use in test code as stimulus.
// Note that it uses CHECKs, which are not acceptable in production code.
class DummyTestContainer {
 public:
  DummyTestContainer() = default;
  ~DummyTestContainer() { Stop(); }

  void Run(int timeout = 60) {
    // First pull the image.
    // Do this separately from running the container, so we can timeout on the true runtime.
    StatusOr<std::string> out = pl::Exec("docker pull " + kImage);
    PL_CHECK_OK(out);
    LOG(INFO) << out.ConsumeValueOrDie();

    // Now run the server.
    // Run with timeout, as a backup in case we don't clean things up properly.
    std::string name = absl::StrCat("dummy_container_",
                                    std::chrono::steady_clock::now().time_since_epoch().count());
    PL_CHECK_OK(container_.Start(
        {"timeout", std::to_string(timeout), "docker", "run", "--rm", "--name", name, kImage}));

    // It takes some time for server to come up, so we keep polling.
    // But keep count of the attempts, because we don't want to poll infinitely.
    int attempts_remaining = kAttempts;

    // Wait for container's server to be running.
    for (; attempts_remaining > 0; --attempts_remaining) {
      sleep(kSleepSeconds);

      // Get the pid of process within the container.
      std::string pid_str =
          pl::Exec(absl::Substitute("docker inspect -f '{{.State.Pid}}' $0", name)).ValueOrDie();
      LOG(INFO) << absl::Substitute("Server PID: $0", pid_str);

      if (absl::SimpleAtoi(pid_str, &process_pid_) && process_pid_ != 0) {
        break;
      }
      process_pid_ = -1;

      // Delay before trying again.
      LOG(INFO) << absl::Substitute(
          "Test Setup: Container not ready, will try again ($0 attempts remaining).",
          attempts_remaining);
    }
    CHECK_GT(attempts_remaining, 0);
    CHECK_NE(process_pid_, -1);

    // Wait for server within container to come up.
    std::string container_out;
    PL_CHECK_OK(container_.Stdout(&container_out));
    while (!absl::StrContains(container_out, "listening on port: 8080")) {
      --attempts_remaining;
      CHECK_GT(attempts_remaining, 0);
      LOG(INFO) << absl::Substitute(
          "Test Setup: Server not ready, will try again ($0 attempts remaining).",
          attempts_remaining);

      sleep(kSleepSeconds);
      PL_CHECK_OK(container_.Stdout(&container_out));
    }
  }

  void Stop() {
    // Clean-up the container.
    container_.Signal(SIGINT);
    container_.Wait();
  }

  int process_pid() { return process_pid_; }

 private:
  // Number of attempts for server to initialize.
  inline static constexpr int kAttempts = 30;

  // Number of seconds to wait between each attempt.
  inline static constexpr int kSleepSeconds = 1;

  // Use the email service from hipster-shop. Really any service will do.
  inline static const std::string kImage =
      "gcr.io/google-samples/microservices-demo/emailservice:v0.1.3";

  SubProcess container_;
  int process_pid_ = -1;
};

}  // namespace pl
