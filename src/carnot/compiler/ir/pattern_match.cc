#include "src/carnot/compiler/ir/pattern_match.h"
#include "src/carnot/compiler/ir/ir_nodes.h"

namespace pl {
namespace carnot {
namespace compiler {

bool CompileTimeIntegerArithmetic::Match(const IRNode* node) const {
  if (!Func().Match(node)) {
    return false;
  }
  auto func = static_cast<const FuncIR*>(node);
  switch (func->opcode()) {
    case FuncIR::Opcode::add:
    // fallthrough
    case FuncIR::Opcode::mult:
    // fallthrough
    case FuncIR::Opcode::sub:
      break;
    default:
      return false;
  }
  if (func->args().size() < 2) {
    return false;
  }
  for (const auto& arg : func->args()) {
    if (ArgMatches(arg)) {
      continue;
    }
    return false;
  }
  return true;
}

bool CompileTimeIntegerArithmetic::ArgMatches(const IRNode* arg) const {
  // TODO(nserrino, philkuz): Generalize this evaluation based on evaluated types of funcs,
  // not hard coding special allowances like it is here/
  return Int().Match(arg) || Match(arg) || CompileTimeUnitTime().Match(arg) ||
         CompileTimeNow().Match(arg);
}

bool CompileTimeNow::Match(const IRNode* node) const {
  if (!Func().Match(node)) {
    return false;
  }
  auto func = static_cast<const FuncIR*>(node);
  return func->carnot_op_name() == kTimeNowFnStr && func->args().size() == 0;
}

bool CompileTimeUnitTime::Match(const IRNode* node) const {
  if (!Func().Match(node)) {
    return false;
  }
  auto func = static_cast<const FuncIR*>(node);
  if (kUnitTimeFnStr.find(func->carnot_op_name()) == kUnitTimeFnStr.end()) {
    return false;
  }
  if (func->args().size() != 1) {
    return false;
  }
  auto arg = func->args()[0];
  return Int().Match(arg) || CompileTimeIntegerArithmetic().Match(arg);
}

}  // namespace compiler
}  // namespace carnot
}  // namespace pl
