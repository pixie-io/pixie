#include "src/common/error.h"
#include "src/common/macros.h"
#include "src/common/status.h"

#include "src/data_collector/data_collector.h"

using pl::datacollector::DataCollector;
using pl::datacollector::EBPFConnector;
using pl::datacollector::OpenTracingConnector;

// A simple wrapper that shows how the data collector is to be hooked up
// In this case, agent and sources are fake.
int main(int argc, char** argv) {
  PL_UNUSED(argc);
  PL_UNUSED(argv);

  std::string bpf_program1_src = "foo";
  std::string kernel_function1 = "kevent_task_switch";
  std::string bpf_program1_entry = "fn_foo";

  std::string bpf_program2_src = "bar";
  std::string kernel_function2 = "kevent_file_open";
  std::string bpf_program2_entry = "fn_bar";

  // Create a data collector;
  DataCollector data_collector;

  // A single eBPF program (e.g. node stats, called on each task_switch_event)
  PL_CHECK_OK(data_collector.AddEBPFSource("EBPF Source 1", bpf_program1_src, kernel_function1,
                                           bpf_program1_entry));

  // A second eBPF program (e.g. file system stats, called on each file_open_event)
  PL_CHECK_OK(data_collector.AddEBPFSource("EBPF Source 2", bpf_program2_src, kernel_function2,
                                           bpf_program2_entry));

  // An OpenTracing source
  PL_CHECK_OK(data_collector.AddOpenTracingSource("OpenTracing Source"));

  // Run Data Collector.
  data_collector.Run();

  // Wait for the thread to return. This should never happen in this example.
  // But don't want the program to terminate.
  data_collector.Wait();

  return 0;
}
