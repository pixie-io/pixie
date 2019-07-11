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
 * if (match(expr, Equals(Column(), Int(10)))) {
 *    // handle case
 *    ...
 * }
 * // Match any int value
 * else if (match(expr, Equals(Column(), Int()))) {
 *    // handle case
 *    ...
 * }
 * // Match any arbitrary value
 * else if (match(expr, Equals(Column(), Value()))) {
 *    // handle case
 *    ...
 * }
 * ```
 *
 * New patterns must fit a specific structure.
 * 1. They must inherit from ParentMatch.
 * 2. They must call the ParentMatch constructor in their own constructor.
 * 3. They must implement match()
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
bool match(Val* V, const Pattern& P) {
  return const_cast<Pattern&>(P).match(V);
}

/**
 * @brief The parent struct to all of the matching structs.
 * Contains an ordering value and a type for
 * easier data structure organization in the future.
 */
struct ParentMatch {
  virtual ~ParentMatch() = default;
  explicit ParentMatch(IRNodeType t) : type(t) {}
  virtual bool match(IRNode* V) const = 0;
  IRNodeType type;
};

/**
 * @brief Match any possible node.
 * It evaluates to true no matter what you throw in there.
 */
struct AllMatch : public ParentMatch {
  AllMatch() : ParentMatch(IRNodeType::kAnyType) {}
  bool match(IRNode*) const override { return true; }
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
  bool match(IRNode* V) const override { return V->type() == type; }
};

// Match an arbitrary Int value.
inline ClassMatch<IRNodeType::IntType> Int() { return ClassMatch<IRNodeType::IntType>(); }

// Match an arbitrary String value.
inline ClassMatch<IRNodeType::StringType> String() { return ClassMatch<IRNodeType::StringType>(); }

/**
 * @brief Match a specific integer value.
 */
struct IntMatch : public ParentMatch {
  int64_t val;
  explicit IntMatch(const int64_t v) : ParentMatch(IRNodeType::IntType), val(v) {}

  bool match(IRNode* V) const override {
    if (V->type() == type) {
      auto iVal = static_cast<IntIR*>(V);
      return iVal->val() == val;
    }
    return false;
  }
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
  LHS_t L;
  RHS_t R;
  bool is_commutable = Commutable;
  FuncIR::Opcode cur_op = op;

  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  BinaryOpMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::FuncType), L(LHS), R(RHS) {}

  bool match(IRNode* V) const override {
    if (V->type() == IRNodeType::FuncType) {
      auto* F = static_cast<FuncIR*>(V);
      if (F->opcode() == cur_op && F->args().size() == 2) {
        return (L.match(F->args()[0]) && R.match(F->args()[1])) ||
               (is_commutable && L.match(F->args()[1]) && R.match(F->args()[0]));
      }
    }
    return false;
  }
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
  LHS_t L;
  RHS_t R;
  bool is_commutable = Commutable;

  // The evaluation order is always stable, regardless of Commutability.
  // The LHS is always matched first.
  AnyBinaryOpMatch(const LHS_t& LHS, const RHS_t& RHS)
      : ParentMatch(IRNodeType::FuncType), L(LHS), R(RHS) {}

  bool match(IRNode* V) const override {
    if (V->type() == type) {
      auto* F = static_cast<FuncIR*>(V);
      if (F->args().size() == 2) {
        return (L.match(F->args()[0]) && R.match(F->args()[1])) ||
               (is_commutable && L.match(F->args()[1]) && R.match(F->args()[0]));
      }
    }
    return false;
  }
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
  ExpressionMatch() : ParentMatch(IRNodeType::kAnyType) {}
  bool match(IRNode* V) const override {
    if (V->IsExpression()) {
      return resolved == static_cast<ExpressionIR*>(V)->IsDataTypeEvaluated();
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
 * @brief Match a specifically typed expression that has a given
 *
 * @tparam expression_type: the type of the node to match (must be an expression).
 * @tparam Resolved: expected resolution of pattern.
 */
template <IRNodeType expression_type, bool Resolved>
struct SpecificExpressionMatch : public ParentMatch {
  SpecificExpressionMatch() : ParentMatch(expression_type) {}
  bool match(IRNode* V) const override {
    if (V->IsExpression() && V->type() == expression_type) {
      return Resolved == static_cast<ExpressionIR*>(V)->IsDataTypeEvaluated();
    }
    return false;
  }
};

/**
 * @brief Match a column that is not resolved.
 */
inline SpecificExpressionMatch<IRNodeType::ColumnType, false> UnresolvedColumn() {
  return SpecificExpressionMatch<IRNodeType::ColumnType, false>();
}
/**
 * @brief Match a column that is resolved.
 */
inline SpecificExpressionMatch<IRNodeType::ColumnType, true> ResolvedColumn() {
  return SpecificExpressionMatch<IRNodeType::ColumnType, true>();
}

/**
 * @brief Match a function that is not resolved.
 */
inline SpecificExpressionMatch<IRNodeType::FuncType, false> UnresolvedFunc() {
  return SpecificExpressionMatch<IRNodeType::FuncType, false>();
}

/**
 * @brief Match a function that is resolved.
 */
inline SpecificExpressionMatch<IRNodeType::FuncType, true> ResolvedFunc() {
  return SpecificExpressionMatch<IRNodeType::FuncType, true>();
}

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
  Arg_t argMatcher_;

  explicit AnyFuncAllArgsMatch(const Arg_t& argMatcher)
      : ParentMatch(IRNodeType::FuncType), argMatcher_(argMatcher) {}

  bool match(IRNode* V) const override {
    if (V->type() == type) {
      auto* F = static_cast<FuncIR*>(V);
      if (Resolved == F->IsDataTypeEvaluated() && CompileTime == F->is_compile_time()) {
        for (const auto a : F->args()) {
          if (!argMatcher_.match(a)) {
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
 * @brief Match any node that is an expression.
 */
struct AnyExpressionMatch : public ParentMatch {
  AnyExpressionMatch() : ParentMatch(IRNodeType::kAnyType) {}
  bool match(IRNode* V) const override { return V->IsExpression(); }
};

/**
 * @brief Match any node that is an expression.
 */
inline AnyExpressionMatch Expression() { return AnyExpressionMatch(); }

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
