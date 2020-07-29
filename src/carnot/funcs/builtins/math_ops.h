#pragma once

#include <cmath>
#include <limits>

#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace builtins {

template <typename TReturn, typename TArg1, typename TArg2>
class AddUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val + b2.val; }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::InheritTypeFromArgs<AddUDF>::Create({types::ST_BYTES}),
    };
  }
};

template <>
class AddUDF<types::StringValue, types::StringValue, types::StringValue> : public udf::ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext*, types::StringValue b1, types::StringValue b2) {
    return b1 + b2;
  }
};

template <typename TReturn, typename TArg1, typename TArg2>
class SubtractUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val - b2.val; }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {
        udf::InheritTypeFromArgs<SubtractUDF>::Create({types::ST_BYTES}),
    };
  }
};

template <typename TReturn, typename TArg1, typename TArg2>
class DivideUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val / b2.val; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class MultiplyUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val * b2.val; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class ModuloUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val % b2.val; }
};

template <typename TArg1, typename TArg2>
class LogicalOrUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val || b2.val; }
};

template <typename TArg1, typename TArg2>
class LogicalAndUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val && b2.val; }
};

template <typename TArg1>
class LogicalNotUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1) { return !b1.val; }
};

template <typename TArg1>
class NegateUDF : public udf::ScalarUDF {
 public:
  TArg1 Exec(FunctionContext*, TArg1 b1) { return -b1.val; }
};

template <typename TArg1>
class InvertUDF : public udf::ScalarUDF {
 public:
  TArg1 Exec(FunctionContext*, TArg1 b1) { return ~b1.val; }
};

template <typename TArg1, typename TArg2>
class EqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 == b2; }
};

template <typename TArg1, typename TArg2>
class NotEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 != b2; }
};

template <typename TArg1, typename TArg2>
class ApproxEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) {
    return std::abs(b1.val - b2.val) < std::numeric_limits<double>::epsilon();
  }
};

template <typename TArg1, typename TArg2>
class ApproxNotEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) {
    return std::abs(b1.val - b2.val) > std::numeric_limits<double>::epsilon();
  }
};

template <typename TArg1, typename TArg2>
class GreaterThanUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 > b2; }
};

template <typename TArg1, typename TArg2>
class GreaterThanEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 >= b2; }
};

template <typename TArg1, typename TArg2>
class LessThanUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 < b2; }
};

template <typename TArg1, typename TArg2>
class LessThanEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 <= b2; }
};

template <typename TReturn, typename TArg1, typename TArg2>
class BinUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val - (b1.val % b2.val); }
};

// Special instantitization to handle float to int conversion
template <typename TReturn, typename TArg2>
class BinUDF<TReturn, types::Float64Value, TArg2> : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, Float64Value b1, Int64Value b2) {
    return static_cast<int64_t>(b1.val) - (static_cast<int64_t>(b1.val) % b2.val);
  }
};

// TODO(philkuz, nserrino) Move decimal places to be a constructor arg after PL-1048 is done.
class RoundUDF : public udf::ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, Float64Value value, Int64Value decimal_places) {
    return absl::StrFormat("%.*f", decimal_places.val, value.val);
  }
};

template <typename TArg>
class MeanUDA : public udf::UDA {
 public:
  void Update(FunctionContext*, TArg arg) {
    size_++;
    count_ += arg.val;
  }
  void Merge(FunctionContext*, const MeanUDA& other) {
    size_ += other.size_;
    count_ += other.count_;
  }
  Float64Value Finalize(FunctionContext*) { return count_ / size_; }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<MeanUDA>::Create({types::ST_BYTES, types::ST_PERCENT})};
  }

 protected:
  uint64_t size_ = 0;
  double count_ = 0;
};

template <typename TArg>
class SumUDA : public udf::UDA {
 public:
  void Update(FunctionContext*, TArg arg) { sum_ = sum_.val + arg.val; }
  void Merge(FunctionContext*, const SumUDA& other) { sum_ = sum_.val + other.sum_.val; }
  TArg Finalize(FunctionContext*) { return sum_; }
  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<SumUDA>::Create({types::ST_BYTES})};
  }

 protected:
  TArg sum_ = 0;
};

template <typename TArg>
class MaxUDA : public udf::UDA {
 public:
  void Update(FunctionContext*, TArg arg) {
    if (max_.val < arg.val) {
      max_ = arg;
    }
  }
  void Merge(FunctionContext*, const MaxUDA& other) {
    if (other.max_.val > max_.val) {
      max_ = other.max_;
    }
  }
  TArg Finalize(FunctionContext*) { return max_; }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<MaxUDA>::Create({types::ST_BYTES, types::ST_PERCENT})};
  }

 protected:
  TArg max_ = std::numeric_limits<typename types::ValueTypeTraits<TArg>::native_type>::min();
};

template <typename TArg>
class MinUDA : public udf::UDA {
 public:
  void Update(FunctionContext*, TArg arg) {
    if (min_.val > arg.val) {
      min_ = arg;
    }
  }
  void Merge(FunctionContext*, const MinUDA& other) {
    if (other.min_.val < min_.val) {
      min_ = other.min_;
    }
  }
  TArg Finalize(FunctionContext*) { return min_; }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<MinUDA>::Create({types::ST_BYTES, types::ST_PERCENT})};
  }

 protected:
  TArg min_ = std::numeric_limits<typename types::ValueTypeTraits<TArg>::native_type>::max();
};

template <typename TArg>
class CountUDA : public udf::UDA {
 public:
  void Update(FunctionContext*, TArg) { count_++; }
  void Merge(FunctionContext*, const CountUDA& other) { count_ += other.count_; }
  Int64Value Finalize(FunctionContext*) { return count_; }

 protected:
  uint64_t count_ = 0;
};

void RegisterMathOpsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
