#include "src/common/subprocess/subprocess.h"

#include "src/common/base/error.h"

namespace pl {

SubProcess::SubProcess(std::vector<std::string> args) : args_(std::move(args)), child_pid_(-1) {}

Status SubProcess::Start() {
  child_pid_ = fork();
  if (child_pid_ < 0) {
    return error::Internal("Could not fork!");
  }
  if (child_pid_ == 0) {
    const char* exe = args_[0].c_str();
    std::vector<char*> args;
    for (size_t i = 1; i < args_.size(); ++i) {
      args.push_back(const_cast<char*>(args_[i].c_str()));
    }
    args.push_back(nullptr);
    int retval = execvp(exe, args.data());
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
