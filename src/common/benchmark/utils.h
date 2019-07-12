#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include "absl/random/zipf_distribution.h"
#include "src/common/base/base.h"
#include "src/shared/types/types.h"

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

template <typename T>
std::vector<T> GetIntsFromExponential(int size, int64_t lambda) {
  std::vector<double> data(size);

  std::random_device rnd_device;
  std::mt19937 mersenne_engine{rnd_device()};  // Generates random integers
  std::exponential_distribution<double> dist(lambda);

  auto gen = [&dist, &mersenne_engine]() { return dist(mersenne_engine); };

  std::generate(begin(data), end(data), gen);
  auto max = *std::max_element(data.begin(), data.end());
  std::vector<T> out_data(size);
  for (size_t i = 0; i < data.size(); i++) {
    out_data[i] = static_cast<int64_t>(data[i] / max * 100);
  }

  return out_data;
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

enum class DistributionType { kUnknown, kUniform, kExponential, kZipfian, kNormal, kConstant };

class DistributionParams {
 public:
  explicit DistributionParams(double new_max) : max_(new_max) {}
  double max() const { return max_; }

 protected:
  double max_;
};

class ZipfianParams : public DistributionParams {
 public:
  double q() const { return q_; }
  double v() const { return v_; }

  ZipfianParams(double zipf_q, double zipf_v, double max_index)
      : DistributionParams(max_index), q_(zipf_q), v_(zipf_v) {}

 private:
  double q_;
  double v_;
};

class NormalParams : public DistributionParams {
 public:
  double mu() const { return mu_; }
  double sigma() const { return sigma_; }

  NormalParams(double normal_sigma, double max_index)
      : DistributionParams(max_index), mu_(max_index / 2), sigma_(normal_sigma) {}

 private:
  double mu_;
  double sigma_;
};

class UniformParams : public DistributionParams {
 public:
  double min() const { return min_; }

  UniformParams(double min_index, double max_index)
      : DistributionParams(max_index), min_(min_index) {}

 private:
  double min_;
};

class IntGenerator {
 public:
  IntGenerator() {}
  virtual ~IntGenerator() {}

  virtual int Generate() = 0;

 protected:
  std::random_device rnd_device_;
  std::mt19937 mersenne_engine{rnd_device_()};  // Generates random integers
  DistributionType len_type_;
};

class ZipfianGenerator : public IntGenerator {
 public:
  explicit ZipfianGenerator(const ZipfianParams* dist_vars) : IntGenerator() {
    q_ = dist_vars->q();
    v_ = dist_vars->v();
    k_ = static_cast<int>(dist_vars->max());
    dist_ = absl::zipf_distribution<int64_t>(k_, q_, v_);
  }

  ~ZipfianGenerator() {}

  int Generate() override { return dist_(mersenne_engine); }

 private:
  absl::zipf_distribution<int64_t> dist_;
  double q_;
  double v_;
  int k_;
};

class UniformGenerator : public IntGenerator {
 public:
  explicit UniformGenerator(const UniformParams* dist_vars) : IntGenerator() {
    new_max_ = static_cast<int>(dist_vars->max());
    new_min_ = static_cast<int>(dist_vars->min());
    dist_ = std::uniform_int_distribution<int64_t>(new_min_, new_max_);
  }
  ~UniformGenerator() {}

  int Generate() override { return dist_(mersenne_engine); }

 private:
  int new_max_;
  int new_min_;
  std::uniform_int_distribution<int64_t> dist_;
};

class NormalGenerator : public IntGenerator {
 public:
  explicit NormalGenerator(const NormalParams* dist_vars) : IntGenerator() {
    sigma_ = dist_vars->sigma();
    mu_ = dist_vars->mu();
    dist_ = std::normal_distribution<double>(mu_, sigma_);
  }

  ~NormalGenerator() {}

  int Generate() override { return static_cast<int>(round(dist_(mersenne_engine))); }

 private:
  double sigma_;
  double mu_;
  std::normal_distribution<double> dist_;
};

class StringGenerator {
 public:
  explicit StringGenerator(IntGenerator* len_gen, IntGenerator* index_gen, double n_strings)
      : strings_(static_cast<int64_t>(n_strings)) {
    len_gen_ = len_gen;
    index_gen_ = index_gen;
    auto gen = [&len_gen]() { return RandomString(len_gen->Generate()); };
    std::generate(begin(strings_), end(strings_), gen);
  }

  std::string NextString() {
    int idx = index_gen_->Generate();
    return strings_[idx];
  }

 private:
  std::random_device rnd_device_;
  IntGenerator* len_gen_;
  IntGenerator* index_gen_;
  std::vector<std::string> strings_;
  std::mt19937 mersenne_engine{rnd_device_()};  // Generates random integers
};

StatusOr<IntGenerator*> IntGenWrapper(DistributionType dist_type,
                                      const DistributionParams* params) {
  IntGenerator* int_gen;
  switch (dist_type) {
    case DistributionType::kZipfian:
      int_gen = new ZipfianGenerator(static_cast<const ZipfianParams*>(params));
      break;
    case DistributionType::kUniform:
      int_gen = new UniformGenerator(static_cast<const UniformParams*>(params));
      break;
    case DistributionType::kNormal:
      int_gen = new NormalGenerator(static_cast<const NormalParams*>(params));
      break;
    default:
      return error::InvalidArgument(
          "We only support Zipf, Normal, and Uniform value distributions.");
  }
  return int_gen;
}

StatusOr<std::vector<types::StringValue>> CreateLargeStringData(
    int size, DistributionType val_dist_type, const DistributionParams* val_dist_vars,
    DistributionType len_type, const DistributionParams* len_dist_vars) {
  std::vector<types::StringValue> data(size);

  IntGenerator* length_generator = IntGenWrapper(len_type, len_dist_vars).ConsumeValueOrDie();
  IntGenerator* index_generator = IntGenWrapper(val_dist_type, val_dist_vars).ConsumeValueOrDie();
  StringGenerator string_gen{length_generator, index_generator,
                             static_cast<double>(val_dist_vars->max())};

  auto gen = [&string_gen]() { return string_gen.NextString(); };

  std::generate(begin(data), end(data), gen);
  return data;
}

}  // namespace bmutils
}  // namespace pl
