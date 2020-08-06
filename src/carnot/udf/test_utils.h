#pragma once
#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include <absl/strings/str_format.h>
#include "src/carnot/udf/udf.h"
#include "src/shared/types/types.h"

namespace pl {
namespace carnot {
namespace udf {

namespace internal {
template <typename T>
void ExpectEquality(const T& v1, const T& v2) {
  EXPECT_EQ(v1.val, v2.val);
}

template <>
void ExpectEquality<types::Float64Value>(const types::Float64Value& v1,
                                         const types::Float64Value& v2) {
  EXPECT_DOUBLE_EQ(v1.val, v2.val);
}

template <>
void ExpectEquality<types::StringValue>(const types::StringValue& v1,
                                        const types::StringValue& v2) {
  EXPECT_EQ(std::string(v1), std::string(v2));
}
}  // namespace internal

/*
 * Test wrapper for testing UDF execution.
 * Example usage:
 *   auto udf_tester = udf::UDFTester<AddUDF<types::Int64Value, types::Int64Value,
 *   types::Int64Value>>(); udf_tester.ForInput(1, 2).Expect(3);
 */
template <typename TUDF>
class UDFTester {
  static constexpr auto udf_data_type = ScalarUDFTraits<TUDF>::ReturnType();

 public:
  UDFTester() {}

  explicit UDFTester(std::unique_ptr<udf::FunctionContext> function_ctx)
      : function_ctx_(std::move(function_ctx)) {}

  /*
   * Execute the UDF on the given arguments and store the result to be checked by Expect.
   * Arguments must be of a type that can usually be passed into the UDF's Exec function,
   * or else there will be an error.
   */
  template <typename... Args>
  UDFTester& ForInput(Args... args) {
    res_ = udf_.Exec(function_ctx_.get(), args...);

    return *this;
  }

  /*
   * Assert that last executed result is equal to the given value.
   * ForInput must be called at least once before Expect is called.
   */
  UDFTester& Expect(typename types::DataTypeTraits<udf_data_type>::value_type arg) {
    internal::ExpectEquality(res_, arg);

    return *this;
  }

 private:
  TUDF udf_;
  std::unique_ptr<udf::FunctionContext> function_ctx_ = nullptr;
  typename types::DataTypeTraits<udf_data_type>::value_type res_;
};

/*
 * Test wrapper for testing UDA finalization and merges.
 * Example usage:
 * auto uda_tester = udf::UDATester<MeanUDA<udf::Float64Value>>();
 * uda_tester.ForInput(1.234).ForInput(2.442).ForInput(1.04).ForInput(5.322).ForInput(6.333).Expect(
 *     expected_mean);
 */
template <typename TUDA>
class UDATester {
  static constexpr auto uda_data_type = UDATraits<TUDA>::FinalizeReturnType();

 public:
  /*
   * Add the given arguments to the UDAs inputs.
   * Arguments must be of a type that can usually be passed into the UDA's Update function,
   * or else there will be an error.
   */
  template <typename... Args>
  UDATester& ForInput(Args... args) {
    uda_.Update(nullptr, args...);

    return *this;
  }

  /*
   * Assert that the finalized result, computed on the UDA's inputs, is equal to the given value.
   */
  UDATester& Expect(typename types::DataTypeTraits<uda_data_type>::value_type arg) {
    internal::ExpectEquality(uda_.Finalize(nullptr), arg);
    return *this;
  }

  /**
   * Returns the result value. Cannot be called after Expect.
   * @return the result value.
   */
  typename types::DataTypeTraits<uda_data_type>::value_type Result() {
    return uda_.Finalize(nullptr);
  }

  /*
   * Merge the UDA from the given UDATester with this UDA.
   */
  UDATester& Merge(UDATester other) {
    uda_.Merge(nullptr, other.uda_);
    return *this;
  }

  types::StringValue Serialize() { return uda_.Serialize(/*ctx*/ nullptr); }

  Status Deserialize(const types::StringValue& data) {
    TUDA other;
    PL_RETURN_IF_ERROR(other.Deserialize(/*ctx*/ nullptr, data));
    uda_.Merge(nullptr, other);
    return Status::OK();
  }

 private:
  TUDA uda_;
};

}  // namespace udf
}  // namespace carnot
}  // namespace pl
