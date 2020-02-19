#include "src/common/base/base.h"
#include "src/stirling/bpf_tools/bcc_wrapper.h"

int main(int argc, char** argv) {
  pl::InitEnvironmentOrDie(&argc, argv);

  pl::ShutdownEnvironmentOrDie();
}
