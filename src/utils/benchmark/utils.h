#pragma once

#include <algorithm>
#include <random>
#include <string>
#include <vector>

namespace pl {
namespace bmutils {

template <typename T>
std::vector<T> CreateLargeData(int size, int64_t min_val = 0, int64_t max_val = 100) {
  std::vector<T> data(size);

  std::random_device rnd_device;
  std::mt19937 mersenne_engine{rnd_device()};  // Generates random integers
  std::uniform_int_distribution<int64_t> dist{min_val, max_val};

  auto gen = [&dist, &mersenne_engine]() { return dist(mersenne_engine); };

  std::generate(begin(data), end(data), gen);
  return data;
}

std::string RandomString(size_t length) {
  auto randchar = []() -> char {
    const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}

}  // namespace bmutils
}  // namespace pl
