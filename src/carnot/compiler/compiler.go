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
// 													 _GoString_ query,
// 													 int* resultLen) {
//   return CompilerCompile(compiler_ptr,
//   											 _GoStringPtr(schema),
//   											 _GoStringLen(schema),
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
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
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
func (cm GoCompiler) Compile(schema *schemapb.Schema, query string) (*pb.CompilerResult, error) {
	var resultLen C.int
	// TODO(philkuz) change this into the serialized (not human readable version) and figure out bytes[] passing.
	schemaStr := proto.MarshalTextString(schema)
	res := C.CompilerCompileGoStr(cm.compiler, schemaStr, query, &resultLen)
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
