#include <gflags/gflags.h>

#include <iostream>

DEFINE_string(message, "", "The message to print");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::cout << "Message: " << FLAGS_message << std::endl;
  return 0;
}
