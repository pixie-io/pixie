/*
 * CPUDistribution Show load distribution across CPU cores during a period of
 *                 time. For Linux, uses BCC, eBPF. Embedded C.
 *
 * Basic example of BCC and kprobes.
 *
 * USAGE: CPUDistribution [duration]
 *
 * Copyright (c) Facebook, Inc.
 * Licensed under the Apache License, Version 2.0 (the "License")
 */
#include <bcc/BPF.h>

#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

extern char _binary_src_primitive_agent_bcc_agent_CPUDistribution_bpf_c_start;

int main(int argc, char** argv) {
  ebpf::BPF bpf;
  char* bpf_prog_ptr = &_binary_src_primitive_agent_bcc_agent_CPUDistribution_bpf_c_start;
  std::string bpf_program = std::string(bpf_prog_ptr);

  auto init_res = bpf.init(bpf_program);
  if (init_res.code() != 0) {
    std::cerr << init_res.msg() << std::endl;
    return 1;
  }

  auto attach_res = bpf.attach_kprobe("finish_task_switch", "task_switch_event");
  if (attach_res.code() != 0) {
    std::cerr << attach_res.msg() << std::endl;
    return 1;
  }

  int probe_time = 10;
  if (argc == 2) {
    probe_time = atoi(argv[1]);
  }
  std::cout << "Probing for " << probe_time << " seconds" << std::endl;
  sleep(probe_time);

  auto table = bpf.get_hash_table<int, uint64_t>("cpu_time");
  auto num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  for (int i = 0; i < num_cores; i++) {
    std::cout << "CPU " << std::setw(2) << i << " worked for ";
    std::cout << (table[i] / 1000000.0) << " ms." << std::endl;
  }

  auto detach_res = bpf.detach_kprobe("finish_task_switch");
  if (detach_res.code() != 0) {
    std::cerr << detach_res.msg() << std::endl;
    return 1;
  }

  return 0;
}
