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

/**
 * This file handles pattern matching. The idea behind this
 * is heavily copied from LLVM's pattern matching module.
 * https://github.com/llvm-mirror/llvm/blob/master/include/llvm/IR/PatternMatch.h
 * access at commit `e30b1c0c22a69971612e014f958ab33916c99f48`.
 *
 * Using the pattern matching interface is very simple.
 *
 * To match `r.latency == 10`, you have several options based on desired specificity,
 * here are a few:
 * ```
 * IRNode* expr; // initialized in the ASTvisitor as a FuncIR.
 * // Most specific
 * if (Match(expr, Equals(Column(), Int(10)))) {
 *    // handle case
 *    ...
 * }
 * // Match any int value
 * else if (Match(expr, Equals(Column(), Int()))) {
 *    // handle case
 *    ...
 * }
 * // Match any arbitrary value
 * else if (Match(expr, Equals(Column(), Value()))) {
 *    // handle case
 *    ...
 * }
 * ```
 *
 * New patterns must fit a specific structure.
 * 1. They must inherit from ParentMatch.
 * 2. They must call the ParentMatch constructor in their own constructor.
 * 3. They must implement Match()
 * 4. To be used properly, they must be specified with a function
 *    - see the Int() fns for an example of what this looks like.
 *
 * Likely for most new patterns you won't need to implement a new struct, but
 * rather you can use an existing struct to fit your use-case.
 */
#pragma once
#include <limits>
#include <string>
#include <unordered_map>

#include "src/carnot/planner/ir/blocking_agg_ir.h"
#include "src/carnot/planner/ir/bool_ir.h"
#include "src/carnot/planner/ir/filter_ir.h"
#include "src/carnot/planner/ir/float_ir.h"
#include "src/carnot/planner/ir/func_ir.h"
#include "src/carnot/planner/ir/grpc_sink_ir.h"
#include "src/carnot/planner/ir/int_ir.h"
#include "src/carnot/planner/ir/ir.h"
#include "src/carnot/planner/ir/join_ir.h"
#include "src/carnot/planner/ir/limit_ir.h"
#include "src/carnot/planner/ir/memory_source_ir.h"
#include "src/carnot/planner/ir/otel_export_sink_ir.h"
#include "src/carnot/planner/ir/string_ir.h"

namespace px {
namespace carnot {
namespace planner {

using shared::metadatapb::MetadataType;

/**
 * @brief Match function that aliases the match function attribute of a pattern.
 */
template <typename Val, typename Pattern>
bool Match(const Val* node, const Pattern& P) {
  return const_cast<Pattern&>(P).Match(node);
}

/**
 * @brief The parent struct to all of the matching structs.
 * Contains an ordering value and a type for
 * easier data structure organization in the future.
 */
struct ParentMatch {
  virtual ~ParentMatch() = default;
  explicit ParentMatch(IRNodeType t) : type(t) {}

  /**
   * @brief Match returns true if the node passed in fits the pattern defined by the struct.
   * @param node: IRNode argument to examine.
   */
  virtual bool Match(const IRNode* node) const = 0;

  IRNodeType type;
};

/**
 * @brief Match any possible node.
 * It evaluates to true no matter what you throw in there.
 */
struct AllMatch : public ParentMatch {
  AllMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode*) const override { return true; }
};

/**
 * @brief Match any valid IRNode.
 */
inline AllMatch Value() { return AllMatch(); }

/**
 * @brief matches
 *
 * @tparam t The IrNodeType
 */
template <IRNodeType t>
struct ClassMatch : public ParentMatch {
  ClassMatch() : ParentMatch(t) {}
  bool Match(const IRNode* node) const override { return node->type() == type; }
};

// Match an arbitrary Int value.
inline ClassMatch<IRNodeType::kInt> Int() { return ClassMatch<IRNodeType::kInt>(); }
inline ClassMatch<IRNodeType::kFloat> Float() { return ClassMatch<IRNodeType::kFloat>(); }
inline ClassMatch<IRNodeType::kBool> Bool() { return ClassMatch<IRNodeType::kBool>(); }
inline ClassMatch<IRNodeType::kUInt128> UInt128() { return ClassMatch<IRNodeType::kUInt128>(); }

// Match an arbitrary String value.
inline ClassMatch<IRNodeType::kString> String() { return ClassMatch<IRNodeType::kString>(); }

// Match an arbitrary Metadata value.
inline ClassMatch<IRNodeType::kMetadata> Metadata() { return ClassMatch<IRNodeType::kMetadata>(); }

// Match an arbitrary Time value.
inline ClassMatch<IRNodeType::kTime> Time() { return ClassMatch<IRNodeType::kTime>(); }

// Match an arbitrary Metadata value.
inline ClassMatch<IRNodeType::kFunc> Func() { return ClassMatch<IRNodeType::kFunc>(); }

inline ClassMatch<IRNodeType::kMemorySource> MemorySource() {
  return ClassMatch<IRNodeType::kMemorySource>();
}
inline ClassMatch<IRNodeType::kMemorySink> MemorySink() {
  return ClassMatch<IRNodeType::kMemorySink>();
}

inline ClassMatch<IRNodeType::kOTelExportSink> OTelExportSink() {
  return ClassMatch<IRNodeType::kOTelExportSink>();
}

inline ClassMatch<IRNodeType::kEmptySource> EmptySource() {
  return ClassMatch<IRNodeType::kEmptySource>();
}
inline ClassMatch<IRNodeType::kLimit> Limit() { return ClassMatch<IRNodeType::kLimit>(); }

inline ClassMatch<IRNodeType::kGRPCSource> GRPCSource() {
  return ClassMatch<IRNodeType::kGRPCSource>();
}
inline ClassMatch<IRNodeType::kGRPCSourceGroup> GRPCSourceGroup() {
  return ClassMatch<IRNodeType::kGRPCSourceGroup>();
}
inline ClassMatch<IRNodeType::kGRPCSink> GRPCSink() { return ClassMatch<IRNodeType::kGRPCSink>(); }

inline ClassMatch<IRNodeType::kJoin> Join() { return ClassMatch<IRNodeType::kJoin>(); }
inline ClassMatch<IRNodeType::kUnion> Union() { return ClassMatch<IRNodeType::kUnion>(); }
inline ClassMatch<IRNodeType::kTabletSourceGroup> TabletSourceGroup() {
  return ClassMatch<IRNodeType::kTabletSourceGroup>();
}

inline ClassMatch<IRNodeType::kGroupBy> GroupBy() { return ClassMatch<IRNodeType::kGroupBy>(); }
inline ClassMatch<IRNodeType::kRolling> Rolling() { return ClassMatch<IRNodeType::kRolling>(); }
inline ClassMatch<IRNodeType::kStream> Stream() { return ClassMatch<IRNodeType::kStream>(); }

inline ClassMatch<IRNodeType::kUDTFSource> UDTFSource() {
  return ClassMatch<IRNodeType::kUDTFSource>();
}
inline ClassMatch<IRNodeType::kUInt128> UInt128Value() {
  return ClassMatch<IRNodeType::kUInt128>();
}

/* Match any source node */
struct Source : public ParentMatch {
  Source() : ParentMatch(IRNodeType::kAny) {}

  bool Match(const IRNode* node) const override {
    return GRPCSource().Match(node) || MemorySource().Match(node) || UDTFSource().Match(node);
  }
};

struct MemorySourceTableMatcher : public ParentMatch {
  explicit MemorySourceTableMatcher(std::string_view table_name)
      : ParentMatch(IRNodeType::kMemorySource), table_name_(std::string(table_name)) {}
  bool Match(const IRNode* node) const override {
    if (!MemorySource().Match(node)) {
      return false;
    }
    auto mem_src = static_cast<const MemorySourceIR*>(node);
    return mem_src->table_name() == table_name_;
  }

  std::string table_name_;
};

inline MemorySourceTableMatcher MemorySource(std::string_view table_name) {
  return MemorySourceTableMatcher(table_name);
}

/* Match any sink node */
struct Sink : public ParentMatch {
  Sink() : ParentMatch(IRNodeType::kAny) {}

  bool Match(const IRNode* node) const override {
    return GRPCSink().Match(node) || MemorySink().Match(node);
  }
};

/* Match GRPCSink with a specific source ID */
struct GRPCSinkWithSourceID : public ParentMatch {
  explicit GRPCSinkWithSourceID(int64_t source_id)
      : ParentMatch(IRNodeType::kGRPCSink), source_id_(source_id) {}
  bool Match(const IRNode* node) const override {
    return GRPCSink().Match(node) &&
           static_cast<const GRPCSinkIR*>(node)->destination_id() == source_id_;
  }

 private:
  int64_t source_id_;
};

/* Match an external GRPC (which produces an output table) */
struct GRPCSinkTypeMatch : public ParentMatch {
  explicit GRPCSinkTypeMatch(bool internal)
      : ParentMatch(IRNodeType::kGRPCSink), internal_(internal) {}

  bool Match(const IRNode* node) const override {
    return GRPCSink().Match(node) &&
           (internal_ ? static_cast<const GRPCSinkIR*>(node)->has_destination_id()
                      : static_cast<const GRPCSinkIR*>(node)->has_output_table());
  }

 private:
  bool internal_;
};

// Matches a GRPC which outputs a final result, streamed to a remote destination.
inline GRPCSinkTypeMatch ExternalGRPCSink() { return GRPCSinkTypeMatch(/* internal */ false); }

// Matches a GRPC which outputs an intermediate result, streamed to another Carnot instance.
inline GRPCSinkTypeMatch InternalGRPCSink() { return GRPCSinkTypeMatch(/* internal */ true); }

// Matches a sink that produces a final (rather than intermediate) result.
struct ResultSink : public ParentMatch {
  ResultSink() : ParentMatch(IRNodeType::kAny) {}

  bool Match(const IRNode* node) const override {
    return ExternalGRPCSink().Match(node) || MemorySink().Match(node) ||
           OTelExportSink().Match(node);
  }
};

/**
 * @brief Match a specific integer value.
 */
struct IntMatch : public ParentMatch {
  explicit IntMatch(const int64_t v) : ParentMatch(IRNodeType::kInt), val(v) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto node_val = static_cast<const IntIR*>(node)->val();
      return node_val == val;
    }
    return false;
  }

  int64_t val;
};

/**
 * @brief Match a specific string value.
 */
struct StringMatch : public ParentMatch {
  explicit StringMatch(const std::string s) : ParentMatch(IRNodeType::kString), val(s) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto node_val = static_cast<const StringIR*>(node)->str();
      return node_val == val;
    }
    return false;
  }

  const std::string val;
};

struct FloatMatch : public ParentMatch {
  explicit FloatMatch(const double v) : ParentMatch(IRNodeType::kFloat), val(v) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto node_val = static_cast<const FloatIR*>(node)->val();
      return std::abs(node_val - val) < std::numeric_limits<double>::epsilon();
    }
    return false;
  }

  double val;
};

struct BoolMatch : public ParentMatch {
  explicit BoolMatch(const bool v) : ParentMatch(IRNodeType::kBool), val(v) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto node_val = static_cast<const BoolIR*>(node)->val();
      return node_val == val;
    }
    return false;
  }

  bool val;
};
/**
 * @brief Match a specific integer value.
 */
inline IntMatch Int(const int64_t val) { return IntMatch(val); }

/**
 * @brief Match a specific integer value.
 */
inline StringMatch String(const std::string val) { return StringMatch(val); }

/**
 * @brief Match a specific integer value.
 */
inline FloatMatch Float(const double val) { return FloatMatch(val); }

/**
 * @brief Match a specific integer value.
 */
inline BoolMatch Bool(const bool val) { return BoolMatch(val); }

/**
 * @brief Match a tablet ID type.
 */
inline ClassMatch<IRNodeType::kString> TabletValue() { return String(); }

/**
 * @brief Match specific binary functions.
 *
 * @tparam LHS_t: the left hand type.
 * @tparam RHS_t: the right hand type.
 * @tparam op: the opcode to match for this Binary operator.
 * @tparam commmutable: whether we can swap left and right arguments.
 */
template <typename LHS_t, typename RHS_t, FuncIR::Opcode op, bool Commutable = false>
struct BinaryOpMatch : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  BinaryOpMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::kFunc), L(LHS), R(RHS) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == IRNodeType::kFunc) {
      auto* F = static_cast<const FuncIR*>(node);
      if (F->opcode() == op && F->all_args().size() == 2) {
        return (L.Match(F->all_args()[0]) && R.Match(F->all_args()[1])) ||
               (Commutable && L.Match(F->all_args()[1]) && R.Match(F->all_args()[0]));
      }
    }
    return false;
  }

  LHS_t L;
  RHS_t R;
};

/**
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::eq, true> Equals(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::eq, true>(L, R);
}

/**
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::add, true> Add(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::add, true>(L, R);
}

/**
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logand, true> LogicalAnd(const LHS& L,
                                                                        const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logand, true>(L, R);
}

/**
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logor, true> LogicalOr(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logor, true>(L, R);
}

inline BinaryOpMatch<AllMatch, AllMatch, FuncIR::Opcode::logand, true> LogicalAnd() {
  return LogicalAnd(Value(), Value());
}

/**
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::lt, false> LessThan(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::lt, false>(L, R);
}

/**
 * @brief Match subtract functions that match the left and right operators. It is notcommutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::sub, false> Subtract(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::sub, false>(L, R);
}

/**
 * @brief Match modulo functions that match the left and right operators. It is notcommutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::mod, false> Modulo(const LHS& L, const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::mod, false>(L, R);
}

/**
 * @brief Match any binary function.
 */
template <typename LHS_t, typename RHS_t, bool Commutable = false>
struct AnyBinaryOpMatch : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  AnyBinaryOpMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::kFunc), L(LHS), R(RHS) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<const FuncIR*>(node);
      if (F->all_args().size() == 2) {
        return (L.Match(F->all_args()[0]) && R.Match(F->all_args()[1])) ||
               (Commutable && L.Match(F->all_args()[1]) && R.Match(F->all_args()[0]));
      }
    }
    return false;
  }

  LHS_t L;
  RHS_t R;
};

/**
 * @brief Matches any BinaryOperation that fits the Left and Right conditions
 * exactly (non-commutative).
 */
template <typename LHS, typename RHS>
inline AnyBinaryOpMatch<LHS, RHS, false> BinOp(const LHS& L, const RHS& R) {
  return AnyBinaryOpMatch<LHS, RHS>(L, R);
}

/**
 * @brief Match any binary op, no need to specify args.
 */
inline AnyBinaryOpMatch<AllMatch, AllMatch, false> BinOp() { return BinOp(Value(), Value()); }

/**
 * @brief Match any expression type.
 */
template <bool resolved>
struct ExpressionMatch : public ParentMatch {
  ExpressionMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    if (node->IsExpression()) {
      return resolved == static_cast<const ExpressionIR*>(node)->IsDataTypeEvaluated();
    }
    return false;
  }
};
/**
 * @brief Match an expression that has been resolved.
 */
inline ExpressionMatch<true> ResolvedExpression() { return ExpressionMatch<true>(); }

/**
 * @brief Match any expression that has not yet been resolved.
 */
inline ExpressionMatch<false> UnresolvedExpression() { return ExpressionMatch<false>(); }

struct ExpressionMatchDataType : public ParentMatch {
  explicit ExpressionMatchDataType(types::DataType type)
      : ParentMatch(IRNodeType::kAny), type_(type) {}
  bool Match(const IRNode* node) const override {
    if (!node->IsExpression()) {
      return false;
    }
    const ExpressionIR* expr = static_cast<const ExpressionIR*>(node);
    return expr->IsDataTypeEvaluated() && expr->EvaluatedDataType() == type_;
  }
  types::DataType type_;
};

inline ExpressionMatchDataType Expression(types::DataType type) {
  return ExpressionMatchDataType(type);
}

/**
 * @brief Matches an expression with a metadata annotation. If a MetadataType is passed in, it
 * must match that particular metadata type.
 *
 */
struct MetadataExpression : public ParentMatch {
  MetadataExpression() : ParentMatch(IRNodeType::kAny) {}
  explicit MetadataExpression(MetadataType type)
      : ParentMatch(IRNodeType::kAny), metadata_type_(type) {}
  bool Match(const IRNode* node) const override {
    if (!node->IsExpression()) {
      return false;
    }
    const ExpressionIR* expr = static_cast<const ExpressionIR*>(node);
    bool type_set = expr->annotations().metadata_type_set();
    if (metadata_type_ == MetadataType::METADATA_TYPE_UNKNOWN) {
      return type_set;
    }
    return type_set && metadata_type_ == expr->annotations().metadata_type;
  }

 private:
  MetadataType metadata_type_ = MetadataType::METADATA_TYPE_UNKNOWN;
};

/**
 * @brief Match a specifically typed expression that has a given resolution state.
 *
 * @tparam expression_type: the type of the node to match (must be an expression).
 * @tparam Resolved: expected resolution of pattern.
 */
template <IRNodeType expression_type, bool Resolved>
struct SpecificExpressionMatch : public ParentMatch {
  SpecificExpressionMatch() : ParentMatch(expression_type) {}
  bool Match(const IRNode* node) const override {
    if (node->IsExpression() && node->type() == expression_type) {
      return Resolved == static_cast<const ExpressionIR*>(node)->IsDataTypeEvaluated();
    }
    return false;
  }
};

/**
 * @brief Match a column that is not resolved.
 */
inline SpecificExpressionMatch<IRNodeType::kColumn, false> UnresolvedColumnType() {
  return SpecificExpressionMatch<IRNodeType::kColumn, false>();
}

/**
 * @brief Match a column that is resolved.
 */
inline SpecificExpressionMatch<IRNodeType::kColumn, true> ResolvedColumnType() {
  return SpecificExpressionMatch<IRNodeType::kColumn, true>();
}

/**
 * @brief Match a function that is not resolved.
 */
inline SpecificExpressionMatch<IRNodeType::kFunc, false> UnresolvedFuncType() {
  return SpecificExpressionMatch<IRNodeType::kFunc, false>();
}

/**
 * @brief Match a function that is resolved.
 */
inline SpecificExpressionMatch<IRNodeType::kFunc, true> ResolvedFuncType() {
  return SpecificExpressionMatch<IRNodeType::kFunc, true>();
}

/**
 * @brief Match metadata ir that has yet to resolve data type.
 */
inline SpecificExpressionMatch<IRNodeType::kMetadata, false> UnresolvedMetadataType() {
  return SpecificExpressionMatch<IRNodeType::kMetadata, false>();
}

/**
 * @brief Match any function with arguments that satisfy argMatcher and matches the specified
 * Resolution and CompileTime values.
 *
 * @tparam Arg_t
 * @tparam false
 * @tparam false
 */
template <typename Arg_t, bool Resolved = false>
struct AnyFuncAllArgsMatch : public ParentMatch {
  explicit AnyFuncAllArgsMatch(const Arg_t& argMatcher)
      : ParentMatch(IRNodeType::kFunc), argMatcher_(argMatcher) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<const FuncIR*>(node);
      if (Resolved == F->IsDataTypeEvaluated()) {
        for (const auto a : F->all_args()) {
          if (!argMatcher_.Match(a)) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  Arg_t argMatcher_;
};

struct FuncNameMatch : public ParentMatch {
  explicit FuncNameMatch(const std::string& name) : ParentMatch(IRNodeType::kFunc), name_(name) {}

  bool Match(const IRNode* node) const override {
    if (!Func().Match(node)) {
      return false;
    }
    auto* func = static_cast<const FuncIR*>(node);
    return func->func_name() == name_;
  }

  std::string name_;
};

inline FuncNameMatch Func(const std::string& name) { return FuncNameMatch(name); }

template <typename Arg_t>
struct FuncNameAllArgsMatch : public ParentMatch {
  FuncNameAllArgsMatch(const std::string& name, const Arg_t& argMatcher)
      : ParentMatch(IRNodeType::kFunc), name_(name), argMatcher_(argMatcher) {}

  bool Match(const IRNode* node) const override {
    if (!Func(name_).Match(node)) {
      return false;
    }
    auto* func = static_cast<const FuncIR*>(node);
    for (const auto a : func->all_args()) {
      if (!argMatcher_.Match(a)) {
        return false;
      }
    }
    return true;
  }

  std::string name_;
  Arg_t argMatcher_;
};

template <typename Arg_t>
inline FuncNameAllArgsMatch<Arg_t> Func(const std::string& name, const Arg_t& argMatcher) {
  return FuncNameAllArgsMatch<Arg_t>(name, argMatcher);
}

/**
 * @brief Matches unresolved & runtime functions with args that satisfy
 * argMatcher.
 *
 * @tparam Arg_t: The type of the argMatcher.
 * @param argMatcher: The pattern that must be satisfied for all arguments.
 */
template <typename Arg_t>
inline AnyFuncAllArgsMatch<Arg_t, false> UnresolvedRTFuncMatchAllArgs(const Arg_t& argMatcher) {
  return AnyFuncAllArgsMatch<Arg_t, false>(argMatcher);
}

/**
 * @brief Match a function with opcode op whose arguments satisfy the arg_matcher.
 *
 * @tparam Arg_t
 * @tparam false
 * @tparam false
 */
template <typename ArgMatcherType, FuncIR::Opcode op>
struct FuncAllArgsMatch : public ParentMatch {
  explicit FuncAllArgsMatch(const ArgMatcherType& arg_matcher)
      : ParentMatch(IRNodeType::kFunc), arg_matcher_(arg_matcher) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto* func = static_cast<const FuncIR*>(node);
      if (func->opcode() == op) {
        for (const auto a : func->all_args()) {
          if (!arg_matcher_.Match(a)) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  ArgMatcherType arg_matcher_;
};

template <typename ArgMatcherType>
inline FuncAllArgsMatch<ArgMatcherType, FuncIR::Opcode::logand> AndFnMatchAll(
    const ArgMatcherType& arg_matcher) {
  return FuncAllArgsMatch<ArgMatcherType, FuncIR::Opcode::logand>(arg_matcher);
}

struct PartialUDAMatch : public ParentMatch {
  PartialUDAMatch() : ParentMatch(IRNodeType::kFunc) {}
  bool Match(const IRNode* node) const override {
    if (!Func().Match(node)) {
      return false;
    }
    const FuncIR* func = static_cast<const FuncIR*>(node);
    return func->SupportsPartial();
  }
};

inline PartialUDAMatch PartialUDA() { return PartialUDAMatch(); }

/**
 * @brief Match any node that is an expression.
 */
struct AnyExpressionMatch : public ParentMatch {
  AnyExpressionMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override { return node->IsExpression(); }
};

/**
 * @brief Match any node that is an expression.
 */
inline AnyExpressionMatch Expression() { return AnyExpressionMatch(); }

/**
 * @brief Match a MemorySource operation that has the expected type status.
 *
 * @tparam HasResolvedType: whether the MemorySource should have a resolved type or not.
 */
template <bool HasResolvedType = false>
struct SourceHasTypeMatch : public ParentMatch {
  SourceHasTypeMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    if (!node->IsOperator()) {
      return false;
    }
    const OperatorIR* op = static_cast<const OperatorIR*>(node);
    return op->IsSource() && op->is_type_resolved() == HasResolvedType;
  }
};

inline SourceHasTypeMatch<false> UnresolvedSource() { return SourceHasTypeMatch<false>(); }
inline SourceHasTypeMatch<true> ResolvedSource() { return SourceHasTypeMatch<true>(); }

struct SourceOperator : public ParentMatch {
  SourceOperator() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    if (!node->IsOperator()) {
      return false;
    }
    const OperatorIR* op = static_cast<const OperatorIR*>(node);
    return op->IsSource();
  }
};

/**
 * @brief Match any operator that matches the type Init status and the parent's
 * type init status.
 *
 * @tparam ResolvedType: whether this operator should have a resolved type.
 * @tparam ParentsOpResolved: whether the parent op should be resolved.
 */
template <bool ResolvedType = false, bool ParentOpResolved = false>
struct AnyTypeResolvedOpMatch : public ParentMatch {
  AnyTypeResolvedOpMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    if (node->IsOperator()) {
      const OperatorIR* op_ir = static_cast<const OperatorIR*>(node);
      if (op_ir->HasParents() && op_ir->is_type_resolved() == ResolvedType) {
        for (OperatorIR* parent : op_ir->parents()) {
          if (parent->is_type_resolved() != ParentOpResolved) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }
};

/**
 * @brief Match an operator of type Matcher that matches the type Init status and the parent's
 * type init status.
 *
 * @tparam Matcher: the type of the matcher for the op.
 * @tparam ResolvedType: whether this operator should have a resolved type.
 * @tparam ParentsOpResolved: whether the parent op type should be resolved.
 */
template <typename Matcher, bool ResolvedType = false, bool ParentOpResolved = false>
struct TypeResolvedOpSpecialMatch : public ParentMatch {
  explicit TypeResolvedOpSpecialMatch(Matcher matcher)
      : ParentMatch(IRNodeType::kAny), matcher_(matcher) {}
  bool Match(const IRNode* node) const override {
    if (matcher_.Match(node)) {
      return AnyTypeResolvedOpMatch<ResolvedType, ParentOpResolved>().Match(node);
    }
    return false;
  }
  Matcher matcher_;
};

/**
 * @brief Match Any operator that doesn't have a resolved type but the parent does.
 */
inline AnyTypeResolvedOpMatch<false, true> UnresolvedReadyOp() {
  return AnyTypeResolvedOpMatch<false, true>();
}

/**
 * @brief Match a Join node that doesn't have a resolved type but it's parents do.
 */
template <typename Matcher>
inline TypeResolvedOpSpecialMatch<Matcher, false, true> UnresolvedReadyOp(Matcher m) {
  return TypeResolvedOpSpecialMatch<Matcher, false, true>(m);
}

struct MatchAnyOp : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  MatchAnyOp() : ParentMatch(IRNodeType::kAny) {}

  bool Match(const IRNode* node) const override { return node->IsOperator(); }
};

inline MatchAnyOp Operator() { return MatchAnyOp(); }

/**
 * @brief Match map operator.
 */
inline ClassMatch<IRNodeType::kMap> Map() { return ClassMatch<IRNodeType::kMap>(); }

/**
 * @brief Match drop operator.
 */
inline ClassMatch<IRNodeType::kDrop> Drop() { return ClassMatch<IRNodeType::kDrop>(); }

/**
 * @brief Match blocking_agg operator.
 */
inline ClassMatch<IRNodeType::kBlockingAgg> BlockingAgg() {
  return ClassMatch<IRNodeType::kBlockingAgg>();
}

template <bool PartialAgg, bool FinalizeAgg>
struct DistributedAggMatcher : public ParentMatch {
  DistributedAggMatcher() : ParentMatch(IRNodeType::kBlockingAgg) {}
  bool Match(const IRNode* node) const override {
    if (!BlockingAgg().Match(node)) {
      return false;
    }
    auto blocking_agg = static_cast<const BlockingAggIR*>(node);
    return blocking_agg->partial_agg() == PartialAgg &&
           blocking_agg->finalize_results() == FinalizeAgg;
  }
};

/**
 * @brief Operator that takes partial aggregates and merges them into a final result.
 */
inline DistributedAggMatcher<false, true> FinalizeAgg() {
  return DistributedAggMatcher<false, true>();
}

/**
 * @brief Node that performs a partial aggregate but does not merge it into a final result.
 */
inline DistributedAggMatcher<true, false> PartialAgg() {
  return DistributedAggMatcher<true, false>();
}

/**
 * @brief Normal logical aggregate.
 *
 */
inline DistributedAggMatcher<true, true> FullAgg() { return DistributedAggMatcher<true, true>(); }

/**
 * @brief Match Filter operator.
 */
inline ClassMatch<IRNodeType::kFilter> Filter() { return ClassMatch<IRNodeType::kFilter>(); }

/* Match Filter with a specific filter expression */
template <typename Matcher>
struct FilterWithExpr : public ParentMatch {
  explicit FilterWithExpr(Matcher matcher) : ParentMatch(IRNodeType::kFilter), matcher_(matcher) {}
  bool Match(const IRNode* node) const override {
    return Filter().Match(node) &&
           matcher_.Match(static_cast<const FilterIR*>(node)->filter_expr());
  }

 private:
  Matcher matcher_;
};

template <typename Matcher>
inline FilterWithExpr<Matcher> Filter(Matcher m) {
  return FilterWithExpr<Matcher>(m);
}

struct ColumnMatch : public ParentMatch {
  ColumnMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    return node->IsExpression() && static_cast<const ExpressionIR*>(node)->IsColumn();
  }
};

inline ColumnMatch ColumnNode() { return ColumnMatch(); }

template <bool MatchName, bool MatchIdx>
struct ColumnPropMatch : public ParentMatch {
  explicit ColumnPropMatch(const std::string& name, int64_t idx)
      : ParentMatch(IRNodeType::kColumn), name_(name), idx_(idx) {}
  bool Match(const IRNode* node) const override {
    if (ColumnNode().Match(node)) {
      const ColumnIR* col_node = static_cast<const ColumnIR*>(node);
      // If matchName, check match name.
      // If MatchIdx, then check the idx.
      return (!MatchName || col_node->col_name() == name_) &&
             (!MatchIdx || col_node->container_op_parent_idx() == idx_);
    }
    return false;
  }
  const std::string& name_;
  int64_t idx_;
};

inline ColumnPropMatch<true, false> ColumnNode(const std::string& name) {
  return ColumnPropMatch<true, false>(name, 0);
}
inline ColumnPropMatch<true, true> ColumnNode(const std::string& name, int64_t parent_idx) {
  return ColumnPropMatch<true, true>(name, parent_idx);
}

struct DataMatch : public ParentMatch {
  DataMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    return node->IsExpression() && static_cast<const ExpressionIR*>(node)->IsData();
  }
};

inline DataMatch DataNode() { return DataMatch(); }

struct BlockingOperatorMatch : public ParentMatch {
  BlockingOperatorMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    return node->IsOperator() && static_cast<const OperatorIR*>(node)->IsBlocking();
  }
};

inline BlockingOperatorMatch BlockingOperator() { return BlockingOperatorMatch(); }

/**
 * @brief Matches two operators in sequence.
 *
 */
template <typename ParentType, typename ChildType>
struct OperatorChainMatch : public ParentMatch {
  OperatorChainMatch(ParentType parent, ChildType child)
      : ParentMatch(IRNodeType::kAny), parent_(parent), child_(child) {}
  bool Match(const IRNode* node) const override {
    if (!node->IsOperator()) {
      return false;
    }
    auto op_node = static_cast<const OperatorIR*>(node);
    if (op_node->Children().size() != 1 || !parent_.Match(op_node)) {
      return false;
    }
    return child_.Match(op_node->Children()[0]);
  }

 private:
  ParentType parent_;
  ChildType child_;
};

template <typename ParentType, typename ChildType>
inline OperatorChainMatch<ParentType, ChildType> OperatorChain(ParentType parent, ChildType child) {
  return OperatorChainMatch(parent, child);
}

template <JoinIR::JoinType Type>
struct JoinMatch : public ParentMatch {
  JoinMatch() : ParentMatch(IRNodeType::kJoin) {}
  bool Match(const IRNode* node) const override {
    if (!Join().Match(node)) {
      return false;
    }
    auto join = static_cast<const JoinIR*>(node);
    return join->join_type() == Type;
  }

 private:
  std::string join_type_;
};

inline JoinMatch<JoinIR::JoinType::kRight> RightJoin() {
  return JoinMatch<JoinIR::JoinType::kRight>();
}

template <typename OpType, typename ParentType>
struct ParentOfOpMatcher : public ParentMatch {
  explicit ParentOfOpMatcher(OpType op_matcher, ParentType parent_matcher)
      : ParentMatch(IRNodeType::kAny), op_matcher_(op_matcher), parent_matcher_(parent_matcher) {}
  bool Match(const IRNode* node) const override {
    if (!op_matcher_.Match(node)) {
      return false;
    }
    // Make sure that we can cast into operator.
    DCHECK(Operator().Match(node));
    auto op = static_cast<const OperatorIR*>(node);
    for (const auto& p : op->parents()) {
      if (!parent_matcher_.Match(p)) {
        return false;
      }
    }
    return true;
  }

  OpType op_matcher_;
  ParentType parent_matcher_;
};
template <typename OpType, typename ParentType>
inline ParentOfOpMatcher<OpType, ParentType> OperatorWithParent(OpType op_matcher,
                                                                ParentType parent_matcher) {
  return ParentOfOpMatcher<OpType, ParentType>(op_matcher, parent_matcher);
}

template <bool OutputColumnsAreSet>
struct OutputColumnsJoinMatcher : public ParentMatch {
  OutputColumnsJoinMatcher() : ParentMatch(IRNodeType::kJoin) {}
  bool Match(const IRNode* node) const override {
    if (!Join().Match(node)) {
      return false;
    }
    auto join = static_cast<const JoinIR*>(node);
    if (OutputColumnsAreSet) {
      return join->output_columns().size() != 0;
    }
    return join->output_columns().size() == 0;
  }
};

inline OutputColumnsJoinMatcher<false> UnsetOutputColumnsJoin() {
  return OutputColumnsJoinMatcher<false>();
}

struct DataOfType : public ParentMatch {
  explicit DataOfType(types::DataType type) : ParentMatch(IRNodeType::kAny), type_(type) {}

  bool Match(const IRNode* node) const override {
    if (!DataNode().Match(node)) {
      return false;
    }
    auto data = static_cast<const DataIR*>(node);
    return data->EvaluatedDataType() == type_;
  }

  types::DataType type_;
};

struct LimitValueMatch : public ParentMatch {
  explicit LimitValueMatch(int64_t limit_value)
      : ParentMatch(IRNodeType::kLimit), value_(limit_value) {}

  bool Match(const IRNode* node) const override {
    if (!Limit().Match(node)) {
      return false;
    }
    auto limit = static_cast<const LimitIR*>(node);
    return limit->limit_value() == value_;
  }

  int64_t value_;
};

inline LimitValueMatch Limit(int64_t limit_value) { return LimitValueMatch(limit_value); }

// Match types::SemanticType in an expression.
struct SemanticTypeMatch : public ParentMatch {
  explicit SemanticTypeMatch(types::SemanticType st) : ParentMatch(IRNodeType::kAny), st_(st) {}

  bool Match(const IRNode* node) const override {
    if (!Expression().Match(node) && !node->is_type_resolved()) {
      return false;
    }
    DCHECK(node->resolved_type()->IsValueType());
    auto value_type = static_cast<ValueType*>(node->resolved_type().get());
    return value_type->semantic_type() == st_;
  }

  types::SemanticType st_;
};

inline SemanticTypeMatch ASID() { return SemanticTypeMatch(types::ST_ASID); }

/**
 * @brief Match a service containment or equality expression.
 *
 * @tparam expression_type: the type of the node to match (must be an expression).
 * @tparam Resolved: expected resolution of pattern.
 */
struct ServiceMatcher : public ParentMatch {
  ServiceMatcher() : ParentMatch(IRNodeType::kAny) {}

  bool Match(const IRNode* node) const override {
    if (!Func("has_service_id").Match(node) && !Func("has_service_name").Match(node)) {
      return false;
    }
    auto func = static_cast<const FuncIR*>(node);
    if (func->all_args().size() != 2) {
      return false;
    }
    auto service_id = MetadataExpression(MetadataType::SERVICE_ID).Match(func->all_args()[0]);
    auto service_name = MetadataExpression(MetadataType::SERVICE_NAME).Match(func->all_args()[0]);
    return (service_id || service_name) && String().Match(func->all_args()[1]);
  }
};

}  // namespace planner
}  // namespace carnot
}  // namespace px
