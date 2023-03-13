/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
// This contains documentation functions for UDFs.
#include <string>
#include <vector>

#include "src/carnot/udf/udf.h"
#include "src/carnot/udfspb/udfs.pb.h"

namespace px {
namespace carnot {
namespace udf {

std::string DedentBlock(const std::string& s);

/**
 * A base documentation builder for a UDF.
 * This class is templated so it can provide a fluent interface.
 */
template <typename T>
class UDFDocBuilder {
 public:
  UDFDocBuilder() = delete;

  /**
   * The details of the document.
   * @param details string describing the details.
   * @return The builder.
   */
  T& Details(std::string_view details) {
    details_ = details;
    return static_cast<T&>(*this);
  }

  /**
   * Args in the order that they appear.
   * @param name The name of the arg.
   * @param desc The description of the arg.
   * @return The builder.
   */
  T& Arg(std::string_view name, std::string_view desc) {
    args_.emplace_back(FuncArg{std::string(name), std::string(desc)});
    return static_cast<T&>(*this);
  }

  /**
   * Documentation for the return.
   * @param returns The documentation for the return value.
   * @return The builder.
   */
  T& Returns(std::string_view returns) {
    returns_ = returns;
    return static_cast<T&>(*this);
  }

  /**
   * String with an example.
   * @param example The example string.
   * @return The builder.
   */
  T& Example(std::string_view example) {
    examples_.emplace_back(std::string(example));
    return static_cast<T&>(*this);
  }

  /**
   * Base protobuf conversion.
   * @param doc The doc proto.
   * @return Status.
   */
  Status ToProtoBase(udfspb::Doc* doc) {
    doc->set_name("");
    doc->set_brief(brief_);
    doc->set_desc(details_);

    for (const auto& example : examples_) {
      auto* ex = doc->add_examples();
      ex->set_value(DedentBlock(example));
    }
    return Status::OK();
  }

 protected:
  explicit UDFDocBuilder(std::string_view brief) : brief_(brief) {}

  struct FuncArg {
    std::string name;
    std::string desc;
  };
  std::string brief_;
  std::string details_;
  std::vector<FuncArg> args_;
  std::string returns_;
  std::vector<std::string> examples_;
};

/**
 * Documentation builder specific to a scalar UDF.
 */
class ScalarUDFDocBuilder : public UDFDocBuilder<ScalarUDFDocBuilder> {
 public:
  explicit ScalarUDFDocBuilder(std::string_view brief) : UDFDocBuilder(brief) {}

  /**
   * Converts the builder to a proto.
   * @tparam TUDF The UDF for the docs.
   * @param doc The doc proto.
   * @return Status of the conversion.
   */
  template <typename TUDF>
  Status ToProto(udfspb::Doc* doc) {
    PX_RETURN_IF_ERROR(UDFDocBuilder::ToProtoBase(doc));

    const auto& init_args = ScalarUDFTraits<TUDF>::InitArguments();
    const auto& exec_args = ScalarUDFTraits<TUDF>::ExecArguments();
    const auto& return_type = ScalarUDFTraits<TUDF>::ReturnType();

    if ((init_args.size() + exec_args.size()) != args_.size()) {
      return error::Internal("Argument count mismatch");
    }

    auto* scalar_doc = doc->mutable_scalar_udf_doc();
    for (const auto& [i, arg] : Enumerate(args_)) {
      auto* a = scalar_doc->add_args();
      a->set_ident(arg.name);
      a->set_desc(arg.desc);
      if (i < init_args.size()) {
        a->set_type(init_args[i]);
      } else {
        a->set_type(exec_args[i - init_args.size()]);
      }
    }

    auto* retval = scalar_doc->mutable_retval();
    retval->set_desc(returns_);
    retval->set_type(return_type);

    return Status::OK();
  }
};

/**
 * UDA specific doc builder.
 */
class UDADocBuilder : public UDFDocBuilder<UDADocBuilder> {
 public:
  UDADocBuilder() = delete;
  explicit UDADocBuilder(std::string_view brief) : UDFDocBuilder(brief) {}

  /**
   * Convert to Proto.
   * @tparam TUDA The UDA.
   * @param doc Doc proto.
   * @return Status of the conversion.
   */
  template <typename TUDA>
  Status ToProto(udfspb::Doc* doc) {
    PX_RETURN_IF_ERROR(UDFDocBuilder::ToProtoBase(doc));

    const auto& init_args = UDATraits<TUDA>::InitArguments();
    const auto& update_args = UDATraits<TUDA>::UpdateArgumentTypes();
    const auto& finalize_type = UDATraits<TUDA>::FinalizeReturnType();

    if ((init_args.size() + update_args.size()) != args_.size()) {
      return error::Internal("Argument count mismatch");
    }

    auto* uda_doc = doc->mutable_uda_doc();
    for (const auto& [i, arg] : Enumerate(args_)) {
      auto* a = uda_doc->add_update_args();
      a->set_ident(arg.name);
      a->set_desc(arg.desc);
      if (i < init_args.size()) {
        a->set_type(init_args[i]);
      } else {
        a->set_type(update_args[i - init_args.size()]);
      }
    }

    auto* retval = uda_doc->mutable_result();
    retval->set_desc(returns_);
    retval->set_type(finalize_type);

    return Status::OK();
  }
};

/**
 * Checks to see if a valid looking ScalarUDF Doc Function exists.
 */
// SFINAE test for doc fn.
template <typename T, typename = void>
struct has_scalar_udf_doc_fn : std::false_type {};

template <typename T>
struct has_scalar_udf_doc_fn<T, std::void_t<decltype(T::Doc)>> : std::true_type {
  static_assert(std::is_same_v<std::invoke_result_t<decltype(T::Doc)>, ScalarUDFDocBuilder>,
                "If a doc functions exists it must have the form: ScalarUDFDocBuilder Doc()");
};

/**
 * Checks to see if a valid looking UDA Doc Function exists.
 */
// SFINAE test for doc fn.
template <typename T, typename = void>
struct has_uda_doc_fn : std::false_type {};

template <typename T>
struct has_uda_doc_fn<T, std::void_t<decltype(T::Doc)>> : std::true_type {
  static_assert(std::is_same_v<std::invoke_result_t<decltype(T::Doc)>, UDADocBuilder>,
                "If a doc functions exists it must have the form: UDADocBuilder Doc()");
};

template <typename T>
constexpr bool has_valid_doc_fn() {
  if constexpr (std::is_base_of_v<ScalarUDF, T>) {
    return has_scalar_udf_doc_fn<T>::value;
  }
  if constexpr (std::is_base_of_v<UDA, T>) {
    return has_uda_doc_fn<T>::value;
  }
  return false;
}

}  // namespace udf
}  // namespace carnot
}  // namespace px
