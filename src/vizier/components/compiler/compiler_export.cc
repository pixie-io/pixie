#include "src/vizier/components/compiler/compiler_export.h"

#include "src/carnot/compiler/compiler.h"

#include <string>

CompilerPtr CompilerNew() {
  auto compiler_ptr = new pl::carnot::compiler::Compiler();
  return reinterpret_cast<CompilerPtr>(compiler_ptr);
}

char *CompilerCompile(CompilerPtr compiler_ptr, const char *rel_map, int rel_map_len,
                      const char *query, int query_len, const char *udf_proto_c_str,
                      int udf_proto_str_len, int *resultLen) {
  PL_UNUSED(compiler_ptr);
  std::string rel_map_str(rel_map, rel_map + rel_map_len);
  std::string query_str(query, query + query_len);
  std::string udf_proto_str(udf_proto_c_str, udf_proto_c_str + udf_proto_str_len);

  // TODO(philkuz) (PL-514) create a CompilerState obj using the relation map
  // and grabbing the current time.
  // TODO(philkuz) (PL-514) pass query into the compile call with the CompilerState.
  // TODO(philkuz) (PL-514) serialize the logical plan into bytes.

  // TODO(philkuz) (PL-514) replace Placeholder to demo that data can transfer between Go and C++.
  std::string ret_str = udf_proto_str + "\t" + rel_map_str + "\t" + query_str;

  *resultLen = ret_str.size();
  char *retval = new char[ret_str.size()];
  memcpy(retval, ret_str.data(), ret_str.size());
  return retval;
}

void CompilerFree(CompilerPtr compiler_ptr) {
  delete reinterpret_cast<pl::carnot::compiler::Compiler *>(compiler_ptr);
  // TODO(philkuz) (PL-514) release the udf_protobuf obj when made.
}

void CompilerStrFree(char *str) { delete str; }
