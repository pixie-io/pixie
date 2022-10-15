//go:build cgo

/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
	"github.com/gogo/protobuf/types"

	// Blank Import required by package.
	_ "github.com/ianlancetaylor/cgosymbolizer"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
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
func (cm GoPlanner) Plan(queryRequest *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error) {
	var resultLen C.int
	queryRequestBytes, err := proto.Marshal(queryRequest)
	if err != nil {
		return nil, err
	}
	queryRequestData := C.CBytes(queryRequestBytes)
	defer C.free(queryRequestData)

	res := C.PlannerPlan(cm.planner, (*C.char)(queryRequestData), C.int(len(queryRequestBytes)), &resultLen)
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

// CompileMutations compiles the query into a mutation of Pixie Data Table.
func (cm GoPlanner) CompileMutations(request *plannerpb.CompileMutationsRequest) (*plannerpb.CompileMutationsResponse, error) {
	var resultLen C.int

	requestBytes, err := proto.Marshal(request)
	if err != nil {
		return nil, err
	}
	requestData := C.CBytes(requestBytes)
	defer C.free(requestData)

	res := C.PlannerCompileMutations(cm.planner, (*C.char)(requestData), C.int(len(requestBytes)), &resultLen)
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

// GenerateOTelScript generates an OTel export script based on the script passed in.
func (cm GoPlanner) GenerateOTelScript(request *plannerpb.GenerateOTelScriptRequest) (*plannerpb.GenerateOTelScriptResponse, error) {
	var resultLen C.int

	requestBytes, err := proto.Marshal(request)
	if err != nil {
		return nil, err
	}
	requestData := C.CBytes(requestBytes)
	defer C.free(requestData)

	res := C.PlannerGenerateOTelScript(cm.planner, (*C.char)(requestData), C.int(len(requestBytes)), &resultLen)
	defer C.StrFree(res)
	resultBytes := C.GoBytes(unsafe.Pointer(res), resultLen)
	if resultLen == 0 {
		return nil, errors.New("no result returned")
	}

	resultPB := &plannerpb.GenerateOTelScriptResponse{}
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
