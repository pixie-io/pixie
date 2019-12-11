#pragma once

struct gostring {
  const char* ptr;
  int64_t len;
};

struct go_byte_array {
  const char* ptr;
  int64_t len;
  int64_t cap;
};

struct go_int32_array {
  const int32_t* ptr;
  int64_t len;
  int64_t cap;
};

struct go_ptr_array {
  const void* ptr;
  int64_t len;
  int64_t cap;
};

struct go_interface {
  int64_t type;
  void* ptr;
};
