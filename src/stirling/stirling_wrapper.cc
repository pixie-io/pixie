#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/common/status.h"
#include "src/stirling/source_registry.h"
#include "src/stirling/stirling.h"

using pl::stirling::SourceRegistry;
using pl::stirling::Stirling;

// A simple wrapper that shows how the data collector is to be hooked up
// In this case, agent and sources are fake.
int main(int argc, char** argv) {
  PL_UNUSED(argc);
  PL_UNUSED(argv);

  // Create a data collector;
  std::unique_ptr<SourceRegistry> registry = std::make_unique<SourceRegistry>("fake_news");
  RegisterFakeSources(registry.get());
  Stirling data_collector(std::move(registry));
  PL_CHECK_OK(data_collector.CreateSourceConnectors());

  // Run Data Collector.
  data_collector.Run();

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  data_collector.Wait();

  return 0;
}
