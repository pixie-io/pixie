package compiler

// #include "src/vizier/components/compiler/compiler_export.h"
//
// CompilerPtr CompilerNewGoStr() {
//   return CompilerNew();
// }
//
// char* CompilerCompileGoStr(CompilerPtr compiler_ptr,
// 													 _GoString_ schema,
// 													 _GoString_ query,
// 													 _GoString_ udf_proto,
// 													 int* resultLen) {
//   return CompilerCompile(compiler_ptr,
//   											 _GoStringPtr(schema),
//   											 _GoStringLen(schema),
//   											 _GoStringPtr(query),
//   											 _GoStringLen(query),
//   											 _GoStringPtr(udf_proto),
//   											 _GoStringLen(udf_proto),
//   											 resultLen);
// }
import "C"
import "unsafe"

// GoCompiler wraps the C Compiler.
type GoCompiler struct {
	compiler C.CompilerPtr
}

// New creates a new GoCompiler object.
func New() GoCompiler {
	var ret GoCompiler
	ret.compiler = C.CompilerNewGoStr()
	return ret
}

// Compile the query with the schema in place.
func (cm GoCompiler) Compile(schema string, query string, udfProto string) string {
	var resultLen C.int
	res := C.CompilerCompileGoStr(cm.compiler, schema, query, udfProto, &resultLen)
	defer C.CompilerStrFree(res)
	lp := C.GoBytes(unsafe.Pointer(res), resultLen)
	return string(lp)
}

// Free the memory used by the compiler.
func (cm GoCompiler) Free() {
	C.CompilerFree(cm.compiler)
}
