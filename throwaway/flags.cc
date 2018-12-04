#include <gflags/gflags.h>
#include <glog/logging.h>

#include <iostream>

DEFINE_string(message, "", "The message to print");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "Message " << FLAGS_message;
  return 0;
}
