// +build !cgo

package goplanner

import (
	"errors"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	statuspb "px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/scriptspb"
)

var errorUnimplemented = errors.New(" ¡UNIMPLEMENTED STUB FOR STATIC ANALYSIS. goplanner ONLY RUNS WITH __CGO__ ENABLED! ")

// GoPlanner wraps the C Planner.
type GoPlanner struct {
}

// New creates a new GoPlanner object.
func New(udfInfo *udfspb.UDFInfo) (GoPlanner, error) {
	return GoPlanner{}, errorUnimplemented
}

// Plan the query with the passed in state, then return the result as a planner result protobuf.
func (cm GoPlanner) Plan(planState *distributedpb.LogicalPlannerState, queryRequest *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error) {
	return nil, errorUnimplemented
}

// GetMainFuncArgsSpec returns the FuncArgSpec of the main function if it exists, otherwise throws a Compiler Error.
func (cm GoPlanner) GetMainFuncArgsSpec(queryRequest *plannerpb.QueryRequest) (*scriptspb.MainFuncSpecResult, error) {
	return nil, errorUnimplemented
}

// ExtractVisFuncsInfo parses a script for misc info such as func args, vega specs, and docstrings.
func (cm GoPlanner) ExtractVisFuncsInfo(script string) (*scriptspb.VisFuncsInfoResult, error) {
	return nil, errorUnimplemented
}

// CompileMutations compiles the query into a mutation of Pixie Data Table.
func (cm GoPlanner) CompileMutations(planState *distributedpb.LogicalPlannerState, request *plannerpb.CompileMutationsRequest) (*plannerpb.CompileMutationsResponse, error) {
	return nil, errorUnimplemented
}

// Free the memory used by the planner.
func (cm GoPlanner) Free() {
}

// GetCompilerErrorContext inspects the context field (pb.Any msg) and
// converts into the CompilerErrorGroup message for proper use.
func GetCompilerErrorContext(status *statuspb.Status, errorPB *compilerpb.CompilerErrorGroup) error {
	return errorUnimplemented
}

// HasContext returns true whether status has context or not.
func HasContext(status *statuspb.Status) bool {
	return false
}
