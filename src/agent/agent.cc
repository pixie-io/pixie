#include "src/common/common.h"

// TODO(zasgar): This file is just a stub for now. Agent code will go here.
int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);
  LOG(INFO) << "Pixie Lab Agent";
  pl::ShutdownEnvironmentOrDie();
}
