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

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Arithmetically add the two arguments.")
        .Details("This function is implicitly invoked by the + operator.")
        .Example("df.sum = df.a + df.b")
        .Arg("arg1", "The value to be added to.")
        .Arg("arg2", "The value to add to the first argument.")
        .Returns("The sum of arg1 and arg2.");
  }
};

template <>
class AddUDF<types::StringValue, types::StringValue, types::StringValue> : public udf::ScalarUDF {
 public:
  types::StringValue Exec(FunctionContext*, types::StringValue b1, types::StringValue b2) {
    return b1 + b2;
  }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Concatenate two strings.")
        .Details("This function is implicitly invoked by the + operator.")
        .Example("df.concat = df.str1 + df.str2")
        .Arg("arg1", "The first string.")
        .Arg("arg2", "The string to append to the first string.")
        .Returns("The concatenation of arg1 and arg2.");
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

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Arithmetically divide the two arguments.")
        .Details("This function is implicitly invoked by the / operator.")
        .Example("df.div = df.a / df.b")
        .Arg("arg1", "The value to divide.")
        .Arg("arg2", "The value to divide the first argument by.")
        .Returns("The value of arg1 divided by arg2.");
  }
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
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Boolean ORs the passed in values.")
        .Example(R"doc(
        | # Implicit operator.
        | df.can_filter_or_has_data = df.can_filter or df.has_data
        | # Explicit call.
        | df.can_filter_or_has_data = px.logicalOr(df.can_filter, df.has_data)
        )doc")
        .Arg("b1", "Left side of the OR.")
        .Arg("b2", "Right side of the OR.")
        .Returns(
            "True if either expression is Truthy or both expressions are Truthy, otherwise False.");
  }
};

template <typename TArg1, typename TArg2>
class LogicalAndUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val && b2.val; }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Boolean ANDs the passed in values.")
        .Example(R"doc(
        | # Implicit operator.
        | df.can_filter_and_has_data = df.can_filter and df.has_data
        | # Explicit call.
        | df.can_filter_and_has_data = px.logicalAnd(df.can_filter, df.has_data)
        )doc")
        .Arg("b1", "Left side of the AND.")
        .Arg("b2", "Right side of the AND.")
        .Returns("True if both expressions are Truthy otherwise False.");
  }
};

template <typename TArg1>
class LogicalNotUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1) { return !b1.val; }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Boolean NOTs the passed in value.")
        .Example(R"doc(
        | # Implicit operator.
        | df.not_can_filter = not df.can_filter
        | # Explicit call.
        | df.not_can_filter = px.logicalNot(df.can_filter)
        )doc")
        .Arg("b1", "The value to Invert.")
        .Returns("True if input is Falsey otherwise False.");
  }
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

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Invert the bits of the given value.")
        .Example("df.inverted = px.invert(df.a)")
        .Arg("arg1", "The value to invert.")
        .Returns("The inverted form of arg1.");
  }
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

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Return whether the two values are approximately equal.")
        .Details(
            "Returns whether or not the two given values are approximately equal to each other, "
            "where the values have an absolute difference of less than 1E-9.")
        .Example("df.equals = px.approxEqual(df.a, df.b)")
        .Arg("arg1", "The value to be compared to.")
        .Arg("arg2", "The value which should be compared to the first argument.")
        .Returns("Boolean of whether the two arguments are approximately equal.");
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

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Compare whether the first argument is greater than the second argument.")
        .Details("This function is implicitly invoked by the > operator.")
        .Example("df.greater = df.a > df.b")
        .Arg("arg1", "The value to be compared to.")
        .Arg("arg2", "The value to check if it is greater than the first argument.")
        .Returns("Boolean of whether arg1 is greater than arg2.");
  }
};

template <typename TArg1, typename TArg2>
class GreaterThanEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 >= b2; }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder(
               "Compare whether the first argument is greater than or equal to the second "
               "argument.")
        .Details("This function is implicitly invoked by the >= operator.")
        .Example("df.geq = df.a >= df.b")
        .Arg("arg1", "The value to be compared to.")
        .Arg("arg2", "The value to check if it is greater than or equal to the first argument.")
        .Returns("Boolean of whether arg1 is greater than or equal to arg2.");
  }
};

template <typename TArg1, typename TArg2>
class LessThanUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 < b2; }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Returns which value is less than the other.")
        .Example("df.cpu1_lessthan_cpu2 = df.cpu1 < df.cpu2")
        .Arg("b1", "Left side of the expression.")
        .Arg("b2", "Right side of the expression.")
        .Returns("Whether the left side is less than the right.");
  }
};

template <typename TArg1, typename TArg2>
class LessThanEqualUDF : public udf::ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1 <= b2; }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Returns which value is less than or equal to the the other.")
        .Example(R"doc(
        | # Implicit operator.
        | df.cpu1_lessthanequal_cpu2 = df.cpu1 <= df.cpu2
        | # Explicit call.
        | df.cpu1_lessthanequal_cpu2 = px.lessThanOrEqual(df.cpu1, df.cup2)
        )doc")
        .Arg("b1", "Left side of the expression.")
        .Arg("b2", "Right side of the expression.")
        .Returns("Whether the left side is less than or equal to the right.");
  }
};

udf::ScalarUDFDocBuilder BinDoc();

template <typename TReturn, typename TArg1, typename TArg2>
class BinUDF : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, TArg1 b1, TArg2 b2) { return b1.val - (b1.val % b2.val); }
  static udf::ScalarUDFDocBuilder Doc() { return BinDoc(); }
};

// Special instantitization to handle float to int conversion
template <typename TReturn, typename TArg2>
class BinUDF<TReturn, types::Float64Value, TArg2> : public udf::ScalarUDF {
 public:
  TReturn Exec(FunctionContext*, Float64Value b1, Int64Value b2) {
    return static_cast<int64_t>(b1.val) - (static_cast<int64_t>(b1.val) % b2.val);
  }
  static udf::ScalarUDFDocBuilder Doc() { return BinDoc(); }
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
    info_.size++;
    info_.count += arg.val;
  }
  void Merge(FunctionContext*, const MeanUDA& other) {
    info_.size += other.info_.size;
    info_.count += other.info_.count;
  }
  Float64Value Finalize(FunctionContext*) { return info_.count / info_.size; }

  static udf::InfRuleVec SemanticInferenceRules() {
    return {udf::InheritTypeFromArgs<MeanUDA>::Create({types::ST_BYTES, types::ST_PERCENT})};
  }

  StringValue Serialize(FunctionContext*) {
    return StringValue(reinterpret_cast<char*>(&info_), sizeof(info_));
  }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    info_ = *reinterpret_cast<const MeanInfo*>(data.data());
    return Status::OK();
  }
  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Calculate the arithmetic mean.")
        .Details(
            "Calculates the arithmetic mean by summing the values then dividing by the number of "
            "values.")
        .Example("df = df.agg(mean=('latency_ms', px.mean))")
        .Arg("arg", "The data to average.")
        .Returns("The mean of the data.");
  }

 protected:
  struct MeanInfo {
    uint64_t size = 0;
    double count = 0;
  };

  MeanInfo info_;
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
  StringValue Serialize(FunctionContext*) {
    return StringValue(reinterpret_cast<char*>(&sum_), sizeof(sum_));
  }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    sum_ =
        *reinterpret_cast<const typename types::ValueTypeTraits<TArg>::native_type*>(data.data());
    return Status::OK();
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

  StringValue Serialize(FunctionContext*) {
    return StringValue(reinterpret_cast<char*>(&max_), sizeof(max_));
  }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    max_ =
        *reinterpret_cast<const typename types::ValueTypeTraits<TArg>::native_type*>(data.data());
    return Status::OK();
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

  StringValue Serialize(FunctionContext*) {
    return StringValue(reinterpret_cast<char*>(&min_), sizeof(min_));
  }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    min_ =
        *reinterpret_cast<const typename types::ValueTypeTraits<TArg>::native_type*>(data.data());
    return Status::OK();
  }
  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Returns the minimum.")
        .Example("df = df.agg(min_latency=('latency_ms', px.min))")
        .Arg("arg", "The data on which to apply the function.")
        .Returns("The minimum.");
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

  StringValue Serialize(FunctionContext*) {
    return StringValue(reinterpret_cast<char*>(&count_), sizeof(count_));
  }

  Status Deserialize(FunctionContext*, const StringValue& data) {
    count_ = *reinterpret_cast<const uint64_t*>(data.data());
    return Status::OK();
  }

  static udf::UDADocBuilder Doc() {
    return udf::UDADocBuilder("Returns number of rows in the aggregate group.")
        .Details(
            "This function counts the number of rows in the aggregate group. The count of "
            "rows is independent of which column this function is applied to. Each column should "
            "return the same number of rows.")
        .Example("df = df.agg(throughput_total=('latency_ms', px.count))")
        .Arg("arg", "The data on which to apply the function.")
        .Returns("The count.");
  }

 protected:
  uint64_t count_ = 0;
};

void RegisterMathOpsOrDie(udf::Registry* registry);
}  // namespace builtins
}  // namespace carnot
}  // namespace pl
