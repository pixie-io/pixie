#pragma once

#define MAX_LOG_MSG_SIZE 256

struct log_event_t {
  struct log_event_attr_t {
    uint32_t msg_size;
  } attr;
  char msg[MAX_LOG_MSG_SIZE];
};
