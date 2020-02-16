package logicalplanner

// // The following is live code even though it is commented out.
// // If you delete it, the planner will break.
// #include "src/carnot/planner/cgo_export.h"
//
// PlannerPtr PlannerNewGoStr(_GoString_ udf_info) {
//   return PlannerNew(_GoStringPtr(udf_info), _GoStringLen(udf_info));
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
//
// char* PlannerGetAvailableFlagsGoStr(PlannerPtr planner_ptr,
// 																			_GoString_ queryRequest, int* resultLen) {
// 	return PlannerGetAvailableFlags(planner_ptr,
// 																	_GoStringPtr(queryRequest),
// 																	_GoStringLen(queryRequest),
// 																	resultLen);
// }
import "C"
import (
	"errors"
	"fmt"
	"unsafe"

	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/udfspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
)

// GoPlanner wraps the C Planner.
type GoPlanner struct {
	planner C.PlannerPtr
}

// New creates a new GoPlanner object.
func New(udfInfo *udfspb.UDFInfo) GoPlanner {
	var ret GoPlanner
	udfInfoStr := proto.MarshalTextString(udfInfo)
	ret.planner = C.PlannerNewGoStr(udfInfoStr)

	return ret
}

// Plan the query with the passed in state, then return the result as a planner result protobuf.
func (cm GoPlanner) Plan(planState *distributedpb.LogicalPlannerState, queryRequest *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error) {
	var resultLen C.int
	// TODO(philkuz) change this into the serialized (not human readable version) and figure out bytes[] passing.
	stateStr := proto.MarshalTextString(planState)
	queryRequestStr := proto.MarshalTextString(queryRequest)
	res := C.PlannerPlanGoStr(cm.planner, stateStr, queryRequestStr, &resultLen)
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

// GetAvailableFlags gets the spec of the flags that a query can accept.
func (cm GoPlanner) GetAvailableFlags(queryRequest *plannerpb.QueryRequest) (*plannerpb.GetAvailableFlagsResult, error) {
	var resultLen C.int
	queryRequestStr := proto.MarshalTextString(queryRequest)
	res := C.PlannerGetAvailableFlagsGoStr(cm.planner, queryRequestStr, &resultLen)
	defer C.StrFree(res)
	resultBytes := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	resultPB := &plannerpb.GetAvailableFlagsResult{}
	if err := proto.Unmarshal(resultBytes, resultPB); err != nil {
		return resultPB, fmt.Errorf("error: '%s'; string: '%s'", err, string(resultBytes))
	}

	return resultPB, nil
}

// Free the memory used by the planner.
func (cm GoPlanner) Free() {
	C.PlannerFree(cm.planner)
}

// GetCompilerErrorContext inspects the context field (pb.Any msg) and
// converts into the CompilerErrorGroup message for proper use.
func GetCompilerErrorContext(status *statuspb.Status, errorPB *compilerpb.CompilerErrorGroup) error {
	context := status.GetContext()
	if context == nil {
		return errors.New("No context in status")
	}

	if !types.Is(context, errorPB) {
		return fmt.Errorf("Didn't expect type %s", context.TypeUrl)
	}
	return types.UnmarshalAny(context, errorPB)
}

// HasContext returns true whether status has context or not.
func HasContext(status *statuspb.Status) bool {
	context := status.GetContext()
	return context != nil
}
