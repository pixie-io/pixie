#include "src/common/base/base.h"
PL_SUPPRESS_WARNINGS_START()
// TODO(michelle): Fix this so that we don't need to the NOLINT.
// NOLINTNEXTLINE(build/include_subdir)
#include "concurrentqueue.h"
PL_SUPPRESS_WARNINGS_END()

int main(int /*argc*/, char** /*argv*/) {
  moodycamel::ConcurrentQueue<int> q;
  q.enqueue(25);

  int item;
  q.try_dequeue(item);
}
