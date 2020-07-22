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
// 													 _GoString_ query_request,
// 													 int* resultLen) {
//   return PlannerPlan(planner_ptr,
//   											 _GoStringPtr(planner_state),
//   											 _GoStringLen(planner_state),
//   											 _GoStringPtr(query_request),
//   											 _GoStringLen(query_request),
//   											 resultLen);
// }
//
// char* PlannerVisFuncsInfoGoStr(PlannerPtr planner_ptr,
//														_GoString_ script,
//														int* resultLen) {
// 	return PlannerVisFuncsInfo(planner_ptr,
//															_GoStringPtr(script),
//															_GoStringLen(script),
//															resultLen);
// }
//
// char* PlannerGetMainFuncArgsSpecGoStr(PlannerPtr planner_ptr,
// 																			_GoString_ queryRequest, int* resultLen) {
// 	return PlannerGetMainFuncArgsSpec(planner_ptr,
// 																	_GoStringPtr(queryRequest),
// 																	_GoStringLen(queryRequest),
// 																	resultLen);
// }
//
// char* PlannerCompileMutationsGoStr(PlannerPtr planner_ptr,
// 						         							  _GoString_ planner_state,
// 						         							  _GoString_ compile_mutation_request,
// 						         							  int* result_len) {
//   return PlannerCompileMutations(planner_ptr,
//   									            	_GoStringPtr(planner_state),
//   									            	_GoStringLen(planner_state),
//   									            	_GoStringPtr(compile_mutation_request),
//   									            	_GoStringLen(compile_mutation_request),
//   									            	result_len);
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
	"pixielabs.ai/pixielabs/src/shared/scriptspb"
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

// GetMainFuncArgsSpec returns the FuncArgSpec of the main function if it exists, otherwise throws a Compiler Error.
func (cm GoPlanner) GetMainFuncArgsSpec(queryRequest *plannerpb.QueryRequest) (*scriptspb.MainFuncSpecResult, error) {
	var resultLen C.int
	queryRequestStr := proto.MarshalTextString(queryRequest)
	res := C.PlannerGetMainFuncArgsSpecGoStr(cm.planner, queryRequestStr, &resultLen)
	defer C.StrFree(res)
	resultBytes := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	resultPB := &scriptspb.MainFuncSpecResult{}
	if err := proto.Unmarshal(resultBytes, resultPB); err != nil {
		return resultPB, fmt.Errorf("error: '%s'; string: '%s'", err, string(resultBytes))
	}

	return resultPB, nil
}

// ExtractVisFuncsInfo parses a script for misc info such as func args, vega specs, and docstrings.
func (cm GoPlanner) ExtractVisFuncsInfo(script string) (*scriptspb.VisFuncsInfoResult, error) {
	var resultLen C.int
	res := C.PlannerVisFuncsInfoGoStr(cm.planner, script, &resultLen)
	defer C.StrFree(res)

	resultBytes := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	visFuncsPb := &scriptspb.VisFuncsInfoResult{}
	if err := proto.Unmarshal(resultBytes, visFuncsPb); err != nil {
		return visFuncsPb, fmt.Errorf("error: '%s'; string: '%s'", err, string(resultBytes))
	}

	return visFuncsPb, nil
}

// CompileMutations compiles the query into a mutation of Pixie Data Table.
func (cm GoPlanner) CompileMutations(planState *distributedpb.LogicalPlannerState, request *plannerpb.CompileMutationsRequest) (*plannerpb.CompileMutationsResponse, error) {
	var resultLen C.int
	stateStr := proto.MarshalTextString(planState)
	requestStr := proto.MarshalTextString(request)
	res := C.PlannerCompileMutationsGoStr(cm.planner, stateStr, requestStr, &resultLen)
	defer C.StrFree(res)
	resultBytes := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	resultPB := &plannerpb.CompileMutationsResponse{}
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
