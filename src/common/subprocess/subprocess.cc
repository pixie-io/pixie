#include "src/common/subprocess/subprocess.h"

#include "src/common/base/error.h"

namespace pl {

SubProcess::SubProcess(std::vector<std::string> args) : args_(std::move(args)), child_pid_(-1) {
  exec_args_.reserve(args_.size() + 1);
  for (const std::string& arg : args_) {
    exec_args_.push_back(const_cast<char*>(arg.c_str()));
  }
  exec_args_.push_back(nullptr);
}

Status SubProcess::Start() {
  child_pid_ = fork();
  if (child_pid_ < 0) {
    return error::Internal("Could not fork!");
  }
  if (child_pid_ == 0) {
    // This will run "ls -la" as if it were a command:
    // char* cmd = "ls";
    // char* argv[3];
    // argv[0] = "ls";
    // argv[1] = "-la";
    // argv[2] = NULL;
    // execvp(cmd, argv);
    int retval = execvp(exec_args_[0], exec_args_.data());
    if (retval == -1) {
      exit(1);
    }
    exit(0);
  } else {
    return Status::OK();
  }
}

void SubProcess::Kill() {
  if (child_pid_ != -1) {
    kill(child_pid_, 9);
  }
}

int SubProcess::Wait() {
  if (child_pid_ != -1) {
    int status = -1;
    waitpid(child_pid_, &status, WUNTRACED);
    return status;
  }
  return 0;
}

}  // namespace pl
