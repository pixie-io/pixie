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
#include <string>

#include "src/carnot/compiler/ir_nodes.h"
namespace pl {
namespace carnot {
namespace compiler {

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

// Match an arbitrary String value.
inline ClassMatch<IRNodeType::kString> String() { return ClassMatch<IRNodeType::kString>(); }

// Match an arbitrary Metadata value.
inline ClassMatch<IRNodeType::kMetadata> Metadata() { return ClassMatch<IRNodeType::kMetadata>(); }

// Match an arbitrary Metadata value.
inline ClassMatch<IRNodeType::kFunc> Func() { return ClassMatch<IRNodeType::kFunc>(); }

inline ClassMatch<IRNodeType::kMemorySource> MemorySource() {
  return ClassMatch<IRNodeType::kMemorySource>();
}
inline ClassMatch<IRNodeType::kMemorySink> MemorySink() {
  return ClassMatch<IRNodeType::kMemorySink>();
}
inline ClassMatch<IRNodeType::kLimit> Limit() { return ClassMatch<IRNodeType::kLimit>(); }

// Match an arbitrary MetadataLiteral value.
inline ClassMatch<IRNodeType::kMetadataLiteral> MetadataLiteral() {
  return ClassMatch<IRNodeType::kMetadataLiteral>();
}

// Match an arbitrary MetadataResolver operator.
inline ClassMatch<IRNodeType::kMetadataResolver> MetadataResolver() {
  return ClassMatch<IRNodeType::kMetadataResolver>();
}
inline ClassMatch<IRNodeType::kGRPCSource> GRPCSource() {
  return ClassMatch<IRNodeType::kGRPCSource>();
}
inline ClassMatch<IRNodeType::kGRPCSourceGroup> GRPCSourceGroup() {
  return ClassMatch<IRNodeType::kGRPCSourceGroup>();
}
inline ClassMatch<IRNodeType::kGRPCSink> GRPCSink() { return ClassMatch<IRNodeType::kGRPCSink>(); }

inline ClassMatch<IRNodeType::kJoin> Join() { return ClassMatch<IRNodeType::kJoin>(); }
inline ClassMatch<IRNodeType::kTabletSourceGroup> TabletSourceGroup() {
  return ClassMatch<IRNodeType::kTabletSourceGroup>();
}

inline ClassMatch<IRNodeType::kList> List() { return ClassMatch<IRNodeType::kList>(); }

/**
 * @brief Match a specific integer value.
 */
struct IntMatch : public ParentMatch {
  explicit IntMatch(const int64_t v) : ParentMatch(IRNodeType::kInt), val(v) {}

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto iVal = static_cast<const IntIR*>(node);
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
 * @brief Match equals functions that match the left and right operators. It is commutative.
 */
template <typename LHS, typename RHS>
inline BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logand, true> LogicalAnd(const LHS& L,
                                                                        const RHS& R) {
  return BinaryOpMatch<LHS, RHS, FuncIR::Opcode::logand, true>(L, R);
}

inline BinaryOpMatch<AllMatch, AllMatch, FuncIR::Opcode::logand, true> LogicalAnd() {
  return LogicalAnd(Value(), Value());
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
 * @brief Match a metadataIR node that has either been Resolved by a metadata
 * resolver node, or not.
 *
 * @tparam Resolved: whether the metadata has been resolved with a resovler node.
 */
template <bool Resolved>
struct MetadataIRMatch : public ParentMatch {
  MetadataIRMatch() : ParentMatch(IRNodeType::kMetadata) {}
  bool Match(const IRNode* node) const override {
    if (node->type() == IRNodeType::kMetadata) {
      return Resolved == static_cast<const MetadataIR*>(node)->HasMetadataResolver();
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

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<const FuncIR*>(node);
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

  bool Match(const IRNode* node) const override {
    if (node->type() == type) {
      auto* F = static_cast<const FuncIR*>(node);
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
        for (const auto a : func->args()) {
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
 * @brief Match a MemorySource operation that has the expected relation status.
 *
 * @tparam HasRelation: whether the MemorySource should have a relation set or not.
 */
template <bool HasRelation = false>
struct SourceHasRelationMatch : public ParentMatch {
  SourceHasRelationMatch() : ParentMatch(IRNodeType::kAny) {}
  bool Match(const IRNode* node) const override {
    if (node->is_source()) {
      return static_cast<const OperatorIR*>(node)->IsRelationInit() == HasRelation;
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
  bool Match(const IRNode* node) const override {
    if (node->IsOperator()) {
      const OperatorIR* op_ir = static_cast<const OperatorIR*>(node);
      if (op_ir->HasParents() && op_ir->IsRelationInit() == ResolvedRelation) {
        for (OperatorIR* parent : op_ir->parents()) {
          if (parent->IsRelationInit() != ParentOpResolved) {
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
  bool Match(const IRNode* node) const override {
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
 * @brief Match a Union node that doesn't have a relation but it's parents do.
 */
inline RelationResolvedOpMatch<IRNodeType::kUnion, false, true> UnresolvedReadyUnion() {
  return RelationResolvedOpMatch<IRNodeType::kUnion, false, true>();
}

/**
 * @brief Match a Join node that doesn't have a relation but it's parents do.
 */
inline RelationResolvedOpMatch<IRNodeType::kJoin, false, true> UnresolvedReadyJoin() {
  return RelationResolvedOpMatch<IRNodeType::kJoin, false, true>();
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

  bool Match(const IRNode* node) const override { return node->IsOperator(); }
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

  bool Match(const IRNode* node) const override {
    if (node->type() == IRNodeType::kRange) {
      auto* r = static_cast<const RangeIR*>(node);
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

  bool Match(const IRNode* node) const override {
    if (node->type() == IRNodeType::kFunc) {
      auto* f = static_cast<const FuncIR*>(node);
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

template <bool ConditionSet = true>
struct JoinOperatorConditionSetMatch : public ParentMatch {
  JoinOperatorConditionSetMatch() : ParentMatch(IRNodeType::kJoin) {}
  bool Match(const IRNode* node) const override {
    if (Join().Match(node)) {
      return static_cast<const JoinIR*>(node)->HasEqualityConditions() == ConditionSet;
    }
    return false;
  }
};

inline JoinOperatorConditionSetMatch<false> JoinOperatorEqCondNotSet() {
  return JoinOperatorConditionSetMatch<false>();
}

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
    DCHECK_LE(op_node->Children().size(), 1UL);
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

template <typename ChildType>
struct ListChildMatch : public ParentMatch {
  explicit ListChildMatch(ChildType child_matcher)
      : ParentMatch(IRNodeType::kList), child_matcher_(child_matcher) {}
  bool Match(const IRNode* node) const override {
    if (!List().Match(node)) {
      return false;
    }
    auto list = static_cast<const ListIR*>(node);
    for (const IRNode* child : list->children()) {
      if (!child_matcher_.Match(child)) {
        return false;
      }
    }
    return true;
  }

  ChildType child_matcher_;
};

/**
 * @brief Matches a list where all elements satisfy the passed in matcher.
 *
 * @tparam ChildType
 * @param child_matcher: the matching function
 * @return matcher to find lists with elements that satisfy the matcher argument.
 */
template <typename ChildType>
inline ListChildMatch<ChildType> ListWithChildren(ChildType child_matcher) {
  return ListChildMatch<ChildType>(child_matcher);
}
}  // namespace compiler
}  // namespace carnot
}  // namespace pl
