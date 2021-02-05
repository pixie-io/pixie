#pragma once

struct stack_trace_key_t {
  unsigned int pid;
  int user_stack_id;
  int kernel_stack_id;
  char name[16 /*TASK_COMM_LEN*/];
};
