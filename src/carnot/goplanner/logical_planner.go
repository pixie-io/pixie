// +build cgo

package goplanner

// // The following is live code even though it is commented out.
// // If you delete it, the planner will break.
// #include <stdlib.h>
// #include "src/carnot/planner/cgo_export.h"
import "C"
import (
	"errors"
	"fmt"
	"unsafe"

	"github.com/gogo/protobuf/proto"

	// Blank Import required by package.
	_ "github.com/ianlancetaylor/cgosymbolizer"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/scriptspb"
	"px.dev/pixie/src/utils/pbutils"
)

// GoPlanner wraps the C Planner.
type GoPlanner struct {
	planner C.PlannerPtr
}

// New creates a new GoPlanner object.
func New(udfInfo *udfspb.UDFInfo) (GoPlanner, error) {
	var ret GoPlanner
	udfInfoBytes, err := proto.Marshal(udfInfo)
	if err != nil {
		return ret, err
	}
	udfInfoData := C.CBytes(udfInfoBytes)
	defer C.free(udfInfoData)

	ret.planner = C.PlannerNew((*C.char)(udfInfoData), C.int(len(udfInfoBytes)))

	return ret, nil
}

// Plan the query with the passed in state, then return the result as a planner result protobuf.
func (cm GoPlanner) Plan(planState *distributedpb.LogicalPlannerState, queryRequest *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error) {
	var resultLen C.int
	stateBytes, err := proto.Marshal(planState)
	if err != nil {
		return nil, err
	}
	stateData := C.CBytes(stateBytes)
	defer C.free(stateData)

	queryRequestBytes, err := proto.Marshal(queryRequest)
	if err != nil {
		return nil, err
	}
	queryRequestData := C.CBytes(queryRequestBytes)
	defer C.free(queryRequestData)

	res := C.PlannerPlan(cm.planner, (*C.char)(stateData), C.int(len(stateBytes)), (*C.char)(queryRequestData), C.int(len(queryRequestBytes)), &resultLen)
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
	queryRequestBytes, err := proto.Marshal(queryRequest)
	if err != nil {
		return nil, err
	}
	queryRequestData := C.CBytes(queryRequestBytes)
	defer C.free(queryRequestData)

	res := C.PlannerGetMainFuncArgsSpec(cm.planner, (*C.char)(queryRequestData), C.int(len(queryRequestBytes)), &resultLen)
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
	scriptData := C.CString(script)
	defer C.free(unsafe.Pointer(scriptData))
	res := C.PlannerVisFuncsInfo(cm.planner, scriptData, C.int(len(script)), &resultLen)
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
	stateBytes, err := proto.Marshal(planState)
	if err != nil {
		return nil, err
	}
	stateData := C.CBytes(stateBytes)
	defer C.free(stateData)

	requestBytes, err := proto.Marshal(request)
	if err != nil {
		return nil, err
	}
	requestData := C.CBytes(requestBytes)
	defer C.free(requestData)

	res := C.PlannerCompileMutations(cm.planner, (*C.char)(stateData), C.int(len(stateBytes)), (*C.char)(requestData), C.int(len(requestBytes)), &resultLen)
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

	if !pbutils.Is(context, errorPB) {
		return fmt.Errorf("Didn't expect type %s", context.TypeUrl)
	}
	return pbutils.UnmarshalAny(context, errorPB)
}

// HasContext returns true whether status has context or not.
func HasContext(status *statuspb.Status) bool {
	context := status.GetContext()
	return context != nil
}
