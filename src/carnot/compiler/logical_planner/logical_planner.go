package logicalplanner

// // The following is live code even though it is commented out.
// // If you delete it, the planner will break.
// #include "src/carnot/compiler/logical_planner/cgo_export.h"
//
// PlannerPtr PlannerNewGoStr() {
//   return PlannerNew();
// }
//
// char* PlannerPlanGoStr(PlannerPtr planner_ptr,
// 													 _GoString_ planner_state,
// 													 _GoString_ query,
// 													 int* resultLen) {
//   return PlannerPlan(planner_ptr,
//   											 _GoStringPtr(planner_state),
//   											 _GoStringLen(planner_state),
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
	"pixielabs.ai/pixielabs/src/carnot/compiler"
	"pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/compiler/distributedpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
)

// GoPlanner wraps the C Planner.
type GoPlanner struct {
	planner C.PlannerPtr
}

// New creates a new GoPlanner object.
func New() GoPlanner {
	var ret GoPlanner

	ret.planner = C.PlannerNewGoStr()

	return ret
}

// Plan the query with the passed in state, then return the result as a planner result protobuf.
func (cm GoPlanner) Plan(planState *distributedpb.LogicalPlannerState, query string) (*distributedpb.LogicalPlannerResult, error) {
	var resultLen C.int
	// TODO(philkuz) change this into the serialized (not human readable version) and figure out bytes[] passing.
	stateStr := proto.MarshalTextString(planState)
	res := C.PlannerPlanGoStr(cm.planner, stateStr, query, &resultLen)
	defer C.StrFree(res)
	lp := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	plan := &distributedpb.LogicalPlannerResult{}
	if err := proto.Unmarshal(lp, plan); err != nil {
		return plan, fmt.Errorf("error: '%s'; string: '%s'", err, string(lp))
	}
	return plan, nil
}

// Free the memory used by the planner.
func (cm GoPlanner) Free() {
	C.PlannerFree(cm.planner)
}

// GetCompilerErrorContext is used in logical_planner_test, which cannot directly call
// compiler since it is a CGO library.
func GetCompilerErrorContext(status *statuspb.Status, errorPB *compilerpb.CompilerErrorGroup) error {
	return compiler.GetCompilerErrorContext(status, errorPB)
}
