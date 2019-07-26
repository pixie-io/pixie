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
#include "src/carnot/compiler/ir_nodes.h"
namespace pl {
namespace carnot {
namespace compiler {

/**
 * @brief Match function that aliases the match function attribute of a pattern.
 */
template <typename Val, typename Pattern>
bool Match(Val* node, const Pattern& P) {
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
  virtual bool Match(IRNode* node) const = 0;

  IRNodeType type;
};

/**
 * @brief Match any possible node.
 * It evaluates to true no matter what you throw in there.
 */
struct AllMatch : public ParentMatch {
  AllMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(IRNode*) const override { return true; }
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
  bool Match(IRNode* node) const override { return node->type() == type; }
};

// Match an arbitrary Int value.
inline ClassMatch<IRNodeType::kInt> Int() { return ClassMatch<IRNodeType::kInt>(); }

// Match an arbitrary String value.
inline ClassMatch<IRNodeType::kString> String() { return ClassMatch<IRNodeType::kString>(); }

// Match an arbitrary Metadata value.
inline ClassMatch<IRNodeType::kMetadata> Metadata() { return ClassMatch<IRNodeType::kMetadata>(); }

// Match an arbitrary MetadataLiteral value.
inline ClassMatch<IRNodeType::kMetadataLiteral> MetadataLiteral() {
  return ClassMatch<IRNodeType::kMetadataLiteral>();
}

// Match an arbitrary MetadataResolver operator.
inline ClassMatch<IRNodeType::kMetadataResolver> MetadataResolver() {
  return ClassMatch<IRNodeType::kMetadataResolver>();
}

/**
 * @brief Match a specific integer value.
 */
struct IntMatch : public ParentMatch {
  explicit IntMatch(const int64_t v) : ParentMatch(IRNodeType::kInt), val(v) {}

  bool Match(IRNode* node) const override {
    if (node->type() == type) {
      auto iVal = static_cast<IntIR*>(node);
      return iVal->val() == val;
    }
    return false;
  }

  int64_t val;
};

/**
 * @brief Match a specific integer value.
 */
inline IntMatch Int(const int64_t val) { return IntMatch(val); }

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

  bool Match(IRNode* node) const override {
    if (node->type() == IRNodeType::kFunc) {
      auto* F = static_cast<FuncIR*>(node);
      if (F->opcode() == op && F->args().size() == 2) {
        return (L.Match(F->args()[0]) && R.Match(F->args()[1])) ||
               (Commutable && L.Match(F->args()[1]) && R.Match(F->args()[0]));
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
 * @brief Match any binary function.
 */
template <typename LHS_t, typename RHS_t, bool Commutable = false>
struct AnyBinaryOpMatch : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  AnyBinaryOpMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::kFunc), L(LHS), R(RHS) {}

  bool Match(IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<FuncIR*>(node);
      if (F->args().size() == 2) {
        return (L.Match(F->args()[0]) && R.Match(F->args()[1])) ||
               (Commutable && L.Match(F->args()[1]) && R.Match(F->args()[0]));
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
  bool Match(IRNode* node) const override {
    if (node->IsExpression()) {
      return resolved == static_cast<ExpressionIR*>(node)->IsDataTypeEvaluated();
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

/**
 * @brief Match a specifically typed expression that has a given resolution state.
 *
 * @tparam expression_type: the type of the node to match (must be an expression).
 * @tparam Resolved: expected resolution of pattern.
 */
template <IRNodeType expression_type, bool Resolved>
struct SpecificExpressionMatch : public ParentMatch {
  SpecificExpressionMatch() : ParentMatch(expression_type) {}
  bool Match(IRNode* node) const override {
    if (node->IsExpression() && node->type() == expression_type) {
      return Resolved == static_cast<ExpressionIR*>(node)->IsDataTypeEvaluated();
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
 * @brief Match a metadataIR node that has either been Resolved by a metadata
 * resolver node, or not.
 *
 * @tparam Resolved: whether the metadata has been resolved with a resovler node.
 */
template <bool Resolved>
struct MetadataIRMatch : public ParentMatch {
  MetadataIRMatch() : ParentMatch(IRNodeType::kMetadata) {}
  bool Match(IRNode* node) const override {
    if (node->type() == IRNodeType::kMetadata) {
      return Resolved == static_cast<MetadataIR*>(node)->HasMetadataResolver();
    }
    return false;
  }
};

/**
 * @brief Match a MetadataIR that doesn't have an associated MetadataResolver node.
 */
inline MetadataIRMatch<false> UnresolvedMetadataIR() { return MetadataIRMatch<false>(); }

/**
 * @brief Match any function with arguments that satisfy argMatcher and matches the specified
 * Resolution and CompileTime values.
 *
 * @tparam Arg_t
 * @tparam false
 * @tparam false
 */
template <typename Arg_t, bool Resolved = false, bool CompileTime = false>
struct AnyFuncAllArgsMatch : public ParentMatch {
  explicit AnyFuncAllArgsMatch(const Arg_t& argMatcher)
      : ParentMatch(IRNodeType::kFunc), argMatcher_(argMatcher) {}

  bool Match(IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<FuncIR*>(node);
      if (Resolved == F->IsDataTypeEvaluated() && CompileTime == F->is_compile_time()) {
        for (const auto a : F->args()) {
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

/**
 * @brief Matches unresolved & runtime functions with args that satisfy
 * argMatcher.
 *
 * @tparam Arg_t: The type of the argMatcher.
 * @param argMatcher: The pattern that must be satisfied for all arguments.
 */
template <typename Arg_t>
inline AnyFuncAllArgsMatch<Arg_t, false, false> UnresolvedRTFuncMatchAllArgs(
    const Arg_t& argMatcher) {
  return AnyFuncAllArgsMatch<Arg_t, false, false>(argMatcher);
}

/**
 * @brief Matches any function that has an argument that matches the passed
 * in matcher and is a compile time function.
 *
 */
template <typename Arg_t, bool CompileTime = false>
struct AnyFuncAnyArgsMatch : public ParentMatch {
  explicit AnyFuncAnyArgsMatch(const Arg_t& argMatcher)
      : ParentMatch(IRNodeType::kFunc), argMatcher_(argMatcher) {}

  bool Match(IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<FuncIR*>(node);
      if (CompileTime == F->is_compile_time()) {
        for (const auto a : F->args()) {
          if (argMatcher_.Match(a)) {
            return true;
          }
        }
      }
    }
    return false;
  }
  Arg_t argMatcher_;
};

/**
 * @brief Matches runtime functions with any arg that satisfies
 * argMatcher.
 *
 * @tparam Arg_t: The type of the argMatcher.
 * @param argMatcher: The pattern that must be satisfied for all arguments.
 */
template <typename Arg_t>
inline AnyFuncAnyArgsMatch<Arg_t, false> FuncAnyArg(const Arg_t& argMatcher) {
  return AnyFuncAnyArgsMatch<Arg_t, false>(argMatcher);
}

/**
 * @brief Match any node that is an expression.
 */
struct AnyExpressionMatch : public ParentMatch {
  AnyExpressionMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(IRNode* node) const override { return node->IsExpression(); }
};

/**
 * @brief Match any node that is an expression.
 */
inline AnyExpressionMatch Expression() { return AnyExpressionMatch(); }

/**
 * @brief Match a MemorySource operation that has the expected relation status.
 *
 * @tparam HasRelation: whether the MemorySource should have a relation set or not.
 */
template <bool HasRelation = false>
struct SourceHasRelationMatch : public ParentMatch {
  SourceHasRelationMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(IRNode* node) const override {
    if (node->is_source()) {
      return static_cast<OperatorIR*>(node)->IsRelationInit() == HasRelation;
    }
    return false;
  }
};

inline SourceHasRelationMatch<false> UnresolvedSource() { return SourceHasRelationMatch<false>(); }
inline SourceHasRelationMatch<true> ResolvedSource() { return SourceHasRelationMatch<true>(); }

/**
 * @brief Match any operator that matches the Relation Init status and the parent's
 * relation init status.
 *
 * @tparam ResolvedRelation: whether this operator should have a resolved relation.
 * @tparam ParentsOpResolved: whether the parent op relation should be resolved.
 */
template <bool ResolvedRelation = false, bool ParentOpResolved = false>
struct AnyRelationResolvedOpMatch : public ParentMatch {
  AnyRelationResolvedOpMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(IRNode* node) const override {
    if (node->IsOp()) {
      OperatorIR* op_ir = static_cast<OperatorIR*>(node);
      if (op_ir->HasParent()) {
        return op_ir->IsRelationInit() == ResolvedRelation &&
               op_ir->parent()->IsRelationInit() == ParentOpResolved;
      }
    }
    return false;
  }
};

/**
 * @brief Match a specific operator that matches the Relation Init status and the parent's
 * relation init status.
 *
 * @tparam op: the type of operator.
 * @tparam ResolvedRelation: whether this operator should have a resolved relation.
 * @tparam ParentsOpResolved: whether the parent op relation should be resolved.
 */
template <IRNodeType op, bool ResolvedRelation = false, bool ParentOpResolved = false>
struct RelationResolvedOpMatch : public ParentMatch {
  RelationResolvedOpMatch() : ParentMatch(op) {}
  bool Match(IRNode* node) const override {
    if (node->type() == op) {
      return AnyRelationResolvedOpMatch<ResolvedRelation, ParentOpResolved>().Match(node);
    }
    return false;
  }
};

/**
 * @brief Match a BlockingAggregate that doesn't have a relation but the parent does.
 */
inline RelationResolvedOpMatch<IRNodeType::kBlockingAgg, false, true> UnresolvedReadyBlockingAgg() {
  return RelationResolvedOpMatch<IRNodeType::kBlockingAgg, false, true>();
}

/**
 * @brief Match a Map that doesn't have a relation but the parent does.
 */
inline RelationResolvedOpMatch<IRNodeType::kMap, false, true> UnresolvedReadyMap() {
  return RelationResolvedOpMatch<IRNodeType::kMap, false, true>();
}

/**
 * @brief Match a MetadataResolver node that doesn't have a relation but the parent does.
 */
inline RelationResolvedOpMatch<IRNodeType::kMetadataResolver, false, true>
UnresolvedReadyMetadataResolver() {
  return RelationResolvedOpMatch<IRNodeType::kMetadataResolver, false, true>();
}

/**
 * @brief Match Any operator that doesn't have a relation but the parent does.
 */
inline AnyRelationResolvedOpMatch<false, true> UnresolvedReadyOp() {
  return AnyRelationResolvedOpMatch<false, true>();
}

struct MatchAnyOp : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  MatchAnyOp() : ParentMatch(IRNodeType::kAny) {}

  bool Match(IRNode* node) const override { return node->IsOp(); }
};

inline MatchAnyOp Operator() { return MatchAnyOp(); }

/**
 * @brief Match Range based on the start stop arguments.
 *
 * @tparam LHS_t: the matcher of the lhs side.
 * @tparam RHS_t: the matcher of the rhs side.
 * @tparam Commutable: whether we can swap lhs and rhs.
 */
template <typename LHS_t, typename RHS_t, bool Commutable = false>
struct RangeArgMatch : public ParentMatch {
  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  RangeArgMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::kRange), L(LHS), R(RHS) {}

  bool Match(IRNode* node) const override {
    if (node->type() == IRNodeType::kRange) {
      auto* r = static_cast<RangeIR*>(node);
      return (L.Match(r->start_repr()) && R.Match(r->stop_repr())) ||
             (Commutable && L.Match(r->start_repr()) && R.Match(r->stop_repr()));
    }
    return false;
  }
  LHS_t L;
  RHS_t R;
};

/**
 * @brief Match range that has (start_repr,stop_repr) match (lhs, rhs).
 */
template <typename LHS_t, typename RHS_t>
inline RangeArgMatch<LHS_t, RHS_t, false> Range(LHS_t lhs, RHS_t rhs) {
  return RangeArgMatch<LHS_t, RHS_t, false>(lhs, rhs);
}

/**
 * @brief Match range operator.
 */
inline ClassMatch<IRNodeType::kRange> Range() { return ClassMatch<IRNodeType::kRange>(); }

/**
 * @brief Match map operator.
 */
inline ClassMatch<IRNodeType::kMap> Map() { return ClassMatch<IRNodeType::kMap>(); }

/**
 * @brief Match blocking_agg operator.
 */
inline ClassMatch<IRNodeType::kBlockingAgg> BlockingAgg() {
  return ClassMatch<IRNodeType::kBlockingAgg>();
}

/**
 * @brief Match Range based on the start stop arguments.
 *
 * @tparam LHS_t: the matcher of the lhs side.
 * @tparam RHS_t: the matcher of the rhs side.
 * @tparam Commutable: whether we can swap lhs and rhs.
 */
template <bool CompileTime = false>
struct FuncMatch : public ParentMatch {
  bool compile_time = CompileTime;

  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  FuncMatch() : ParentMatch(IRNodeType::kFunc) {}

  bool Match(IRNode* node) const override {
    if (node->type() == IRNodeType::kFunc) {
      auto* f = static_cast<FuncIR*>(node);
      return f->is_compile_time() == compile_time;
    }
    return false;
  }
};

/**
 * @brief Match compile-time function.
 */
inline FuncMatch<true> CompileTimeFunc() { return FuncMatch<true>(); }

/**
 * @brief Match run-time function.
 */
inline FuncMatch<false> RunTimeFunc() { return FuncMatch<false>(); }

/**
 * @brief Match Filter operator.
 */
inline ClassMatch<IRNodeType::kFilter> Filter() { return ClassMatch<IRNodeType::kFilter>(); }
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
