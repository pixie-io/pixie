#pragma once

#include <cmath>
#include <limits>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/udf.h"

namespace pl {
namespace carnot {
namespace builtins {

template <typename TReturn, typename TArg1, typename TArg2>
class AddUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val + b2.val; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class SubtractUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val - b2.val; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class DivideUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val / b2.val; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class MultiplyUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val * b2.val; }
};

class ModuloUDF : public udf::ScalarUDF {
 public:
  udf::Int64Value Exec(udf::FunctionContext *, udf::Int64Value b1, udf::Int64Value b2) {
    return b1.val % b2.val;
  }
};

template <typename TArg1, typename TArg2>
class LogicalOrUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val || b2.val; }
};

template <typename TArg1, typename TArg2>
class LogicalAndUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1.val && b2.val; }
};

template <typename TArg1>
class LogicalNotUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1) { return !b1.val; }
};

template <typename TArg1>
class NegateUDF : public udf::ScalarUDF {
 public:
  TArg1 Exec(udf::FunctionContext *, TArg1 b1) { return -b1.val; }
};

template <typename TArg1, typename TArg2>
class EqualUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1 == b2; }
};

template <typename TArg1, typename TArg2>
class ApproxEqualUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) {
    return std::abs(b1.val - b2.val) < std::numeric_limits<double>::epsilon();
  }
};

template <typename TArg1, typename TArg2>
class GreaterThanUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1 > b2; }
};

template <typename TArg1, typename TArg2>
class LessThanUDF : public udf::ScalarUDF {
 public:
  udf::BoolValue Exec(udf::FunctionContext *, TArg1 b1, TArg2 b2) { return b1 < b2; }
};

template <typename TArg>
class MeanUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg arg) {
    size_++;
    count_ += arg.val;
  }
  void Merge(udf::FunctionContext *, const MeanUDA &other) {
    size_ += other.size_;
    count_ += other.count_;
  }
  udf::Float64Value Finalize(udf::FunctionContext *) { return count_ / size_; }

 protected:
  uint64_t size_ = 0;
  double count_ = 0;
};

template <typename TArg>
class SumUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg arg) { sum_ = sum_.val + arg.val; }
  void Merge(udf::FunctionContext *, const SumUDA &other) { sum_ = sum_.val + other.sum_.val; }
  TArg Finalize(udf::FunctionContext *) { return sum_; }

 protected:
  TArg sum_ = 0;
};

template <typename TArg>
class MaxUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg arg) {
    if (max_.val < arg.val) {
      max_ = arg;
    }
  }
  void Merge(udf::FunctionContext *, const MaxUDA &other) {
    if (other.max_.val > max_.val) {
      max_ = other.max_;
    }
  }
  TArg Finalize(udf::FunctionContext *) { return max_; }

 protected:
  TArg max_ = std::numeric_limits<typename udf::UDFValueTraits<TArg>::native_type>::min();
};

template <typename TArg>
class MinUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg arg) {
    if (min_.val > arg.val) {
      min_ = arg;
    }
  }
  void Merge(udf::FunctionContext *, const MinUDA &other) {
    if (other.min_.val < min_.val) {
      min_ = other.min_;
    }
  }
  TArg Finalize(udf::FunctionContext *) { return min_; }

 protected:
  TArg min_ = std::numeric_limits<typename udf::UDFValueTraits<TArg>::native_type>::max();
};

template <typename TArg>
class CountUDA : public udf::UDA {
 public:
  void Update(udf::FunctionContext *, TArg) { count_++; }
  void Merge(udf::FunctionContext *, const CountUDA &other) { count_ += other.count_; }
  udf::Int64Value Finalize(udf::FunctionContext *) { return count_; }

 protected:
  uint64_t count_ = 0;
};

void RegisterMathOpsOrDie(udf::ScalarUDFRegistry *registry);
void RegisterMathOpsOrDie(udf::UDARegistry *registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
