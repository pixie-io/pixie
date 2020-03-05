#include "src/common/testing/test_utils/container_runner.h"

#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"

namespace pl {

Status ContainerRunner::Run(int timeout, const std::vector<std::string>& options) {
  // First pull the image.
  // Do this separately from running the container, so we can timeout on the true runtime.
  PL_ASSIGN_OR_RETURN(std::string out, pl::Exec("docker pull " + image_));
  LOG(INFO) << out;

  // Now run the server.
  // Run with timeout, as a backup in case we don't clean things up properly.
  container_name_ = absl::StrCat(instance_name_prefix_, "_",
                                 std::chrono::steady_clock::now().time_since_epoch().count());

  std::vector<std::string> docker_run_cmd;
  docker_run_cmd.push_back("timeout");
  docker_run_cmd.push_back(std::to_string(timeout));
  docker_run_cmd.push_back("docker");
  docker_run_cmd.push_back("run");
  docker_run_cmd.push_back("--rm");
  docker_run_cmd.push_back("--pid=host");
  for (const auto& flag : options) {
    docker_run_cmd.push_back(flag);
  }
  docker_run_cmd.push_back("--name");
  docker_run_cmd.push_back(container_name_);
  docker_run_cmd.push_back(image_);

  PL_RETURN_IF_ERROR(container_.Start(docker_run_cmd));

  // It may take some time for the container to come up, so we keep polling.
  // But keep count of the attempts, because we don't want to poll infinitely.
  int attempts_remaining = timeout;

  // Wait for container's server to be running.
  for (; attempts_remaining > 0; --attempts_remaining) {
    sleep(kSleepSeconds);

    // Get the pid of process within the container.
    PL_ASSIGN_OR_RETURN(
        std::string pid_str,
        pl::Exec(absl::Substitute("docker inspect -f '{{.State.Pid}}' $0", container_name_)));
    LOG(INFO) << absl::Substitute("Container process PID: $0", pid_str);

    if (absl::SimpleAtoi(pid_str, &process_pid_) && process_pid_ != 0) {
      break;
    }
    process_pid_ = -1;

    // Delay before trying again.
    LOG(INFO) << absl::Substitute(
        "Container not yet running, will try again ($0 attempts remaining).", attempts_remaining);
  }

  if (process_pid_ == -1) {
    return error::Internal("Timeout. Container did not start.");
  }
  DCHECK_GT(attempts_remaining, 0);

  LOG(INFO) << absl::StrCat("Waiting for log message: ", ready_message_);

  // Wait for container to become "ready".
  std::string container_out;
  PL_RETURN_IF_ERROR(container_.Stdout(&container_out));
  while (!absl::StrContains(container_out, ready_message_)) {
    sleep(kSleepSeconds);
    PL_RETURN_IF_ERROR(container_.Stdout(&container_out));

    --attempts_remaining;
    if (attempts_remaining <= 0) {
      return error::Internal("Timeout. Container did not reach ready state.");
    }
    LOG(INFO) << absl::Substitute(
        "Container not in ready state, will try again ($0 attempts remaining).",
        attempts_remaining);
  }

  LOG(INFO) << absl::Substitute("Container $0 is ready.", container_name_);
  return Status::OK();
}

/**
 * Stops the container by sending it an interrupt signal.
 */
void ContainerRunner::Stop() {
  // Clean-up the container.
  container_.Signal(SIGINT);
  container_.Wait();
}

}  // namespace pl
