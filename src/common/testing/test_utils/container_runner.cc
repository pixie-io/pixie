#include "src/common/testing/test_utils/container_runner.h"

#include "src/common/exec/exec.h"
#include "src/common/exec/subprocess.h"

namespace pl {

ContainerRunner::ContainerRunner(std::string_view image, std::string_view instance_name_prefix,
                                 std::string_view ready_message)
    : image_(image), instance_name_prefix_(instance_name_prefix), ready_message_(ready_message) {
  std::string out = pl::Exec("docker pull " + image_).ConsumeValueOrDie();
  LOG(INFO) << out;
}

ContainerRunner::ContainerRunner(std::filesystem::path image_tar,
                                 std::string_view instance_name_prefix,
                                 std::string_view ready_message)
    : instance_name_prefix_(instance_name_prefix), ready_message_(ready_message) {
  std::string docker_load_cmd = absl::Substitute("docker load -i $0", image_tar.string());
  VLOG(1) << docker_load_cmd;
  std::string out = pl::Exec(docker_load_cmd).ConsumeValueOrDie();
  LOG(INFO) << out;

  // Extract the image name.
  std::vector<std::string_view> lines = absl::StrSplit(out, "\n", absl::SkipWhitespace());
  CHECK(!lines.empty());
  std::string_view image_line = lines.back();
  constexpr std::string_view kLoadedImagePrefix = "Loaded image: ";
  CHECK(absl::StartsWith(image_line, kLoadedImagePrefix));
  image_line.remove_prefix(kLoadedImagePrefix.length());
  image_ = image_line;
}

StatusOr<std::string> ContainerRunner::Run(int timeout, const std::vector<std::string>& options) {
  // Now run the container.
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

  LOG(INFO) << docker_run_cmd;
  PL_RETURN_IF_ERROR(container_.Start(docker_run_cmd, /* stderr_to_stdout */ true));

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

    // TODO(oazizi): Check if container execution has failed, and abort early.
  }

  if (process_pid_ == -1) {
    std::string container_out;
    PL_RETURN_IF_ERROR(container_.Stdout(&container_out));
    return error::Internal("Container failed to start. Container output:\n$0", container_out);
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
  return container_out;
}

void ContainerRunner::Stop() {
  // Clean-up the container.
  container_.Signal(SIGTERM);
  container_.Wait();
}

void ContainerRunner::Wait() { container_.Wait(); }

}  // namespace pl
