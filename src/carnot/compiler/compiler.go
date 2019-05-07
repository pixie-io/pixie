package compiler

// // The following is live code even though it is commented out.
// // If you delete it, the compiler will break.
// #include "src/carnot/compiler/compiler_export.h"
//
// CompilerPtr CompilerNewGoStr() {
//   return CompilerNew();
// }
//
// char* CompilerCompileGoStr(CompilerPtr compiler_ptr,
// 													 _GoString_ schema,
// 													 _GoString_ table_name,
// 													 _GoString_ query,
// 													 int* resultLen) {
//   return CompilerCompile(compiler_ptr,
//   											 _GoStringPtr(schema),
//   											 _GoStringLen(schema),
//   											 _GoStringPtr(table_name),
//   											 _GoStringLen(table_name),
//   											 _GoStringPtr(query),
//   											 _GoStringLen(query),
//   											 resultLen);
// }
import "C"
import (
	"errors"
	"fmt"
	"unsafe"

	"github.com/gogo/protobuf/proto"
	pb "pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
)

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

// Compile the query with the schema in place, then return the result as a logical plan protobuf.
func (cm GoCompiler) Compile(schema, tableName, query string) (*pb.CompilerResult, error) {
	var resultLen C.int
	// TODO(reviewer) what if we also pass a "error" bool pointer that keeps
	// track of whether the result is an error or not?
	// That way we don't have to unmarshal before we find out there's an error.
	res := C.CompilerCompileGoStr(cm.compiler, schema, tableName, query, &resultLen)
	defer C.CompilerStrFree(res)
	lp := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	plan := &pb.CompilerResult{}
	if err := proto.Unmarshal(lp, plan); err != nil {
		return plan, fmt.Errorf("error: '%s'; string: '%s'", err, string(lp))
	}
	return plan, nil
}

// Free the memory used by the compiler.
func (cm GoCompiler) Free() {
	C.CompilerFree(cm.compiler)
}
