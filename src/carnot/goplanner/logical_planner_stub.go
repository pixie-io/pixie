//go:build !cgo

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

import (
	"errors"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/common/base/statuspb"
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
func (cm GoPlanner) Plan(queryRequest *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error) {
	return nil, errorUnimplemented
}

// CompileMutations compiles the query into a mutation of Pixie Data Table.
func (cm GoPlanner) CompileMutations(request *plannerpb.CompileMutationsRequest) (*plannerpb.CompileMutationsResponse, error) {
	return nil, errorUnimplemented
}

// GenerateOTelScript generates an OTel script from the query.
func (cm GoPlanner) GenerateOTelScript(request *plannerpb.GenerateOTelScriptRequest) (*plannerpb.GenerateOTelScriptResponse, error) {
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
