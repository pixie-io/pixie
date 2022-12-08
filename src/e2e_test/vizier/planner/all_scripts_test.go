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

package planner_test

import (
	"errors"
	"fmt"
	"io"
	"os"
	"path"
	"path/filepath"
	"strings"
	"testing"

	"github.com/bazelbuild/rules_go/go/tools/bazel"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/e2e_test/vizier/planner/dump_schemas/godumpschemas"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	funcs "px.dev/pixie/src/vizier/funcs/go"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

const scriptDir = "src/pxl_scripts"

type script struct {
	name    string
	pxlPath string
	visPath string
}

func collectAllScripts() ([]*script, error) {
	dirToScript := make(map[string]*script)

	scriptDirAbsPath, err := bazel.Runfile(scriptDir)
	if err != nil {
		return nil, errors.New("Must run test through bazel, so that it can find the pxl scripts: ")
	}
	pxlPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*.pxl"))
	if err != nil {
		return nil, err
	}
	otherPxlPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*", "*.pxl"))
	if err != nil {
		return nil, err
	}
	pxlPaths = append(pxlPaths, otherPxlPaths...)
	for _, p := range pxlPaths {
		dirname := filepath.Dir(p)
		name := path.Join(filepath.Base(filepath.Dir(dirname)), filepath.Base(dirname))
		dirToScript[dirname] = &script{name: name, pxlPath: p}
	}
	visPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*.json"))
	if err != nil {
		return nil, err
	}
	otherVisPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*", "*.json"))
	if err != nil {
		return nil, err
	}
	visPaths = append(visPaths, otherVisPaths...)
	for _, p := range visPaths {
		dirname := filepath.Dir(p)
		s, ok := dirToScript[dirname]
		if !ok {
			return nil, fmt.Errorf("Found vis.json without pxl script: %s", p)
		}
		s.visPath = p
	}

	scripts := make([]*script, 0, len(dirToScript))
	for _, s := range dirToScript {
		scripts = append(scripts, s)
	}
	return scripts, nil
}

func defaultForType(pxType vispb.PXType) string {
	switch pxType {
	case vispb.PX_UNKNOWN:
		return ""
	case vispb.PX_BOOLEAN:
		return "True"
	case vispb.PX_INT64:
		return "1"
	case vispb.PX_FLOAT64:
		return "1.0"
	case vispb.PX_STRING:
		return ""
	case vispb.PX_SERVICE, vispb.PX_POD, vispb.PX_CONTAINER, vispb.PX_NAMESPACE, vispb.PX_NODE:
		return "pl"
	case vispb.PX_LIST:
		return "[]"
	case vispb.PX_STRING_LIST:
		return "[\"\"]"
	default:
		return ""
	}
}

func getArgsFromVis(execFunc *plannerpb.FuncToExecute, args []*vispb.Widget_Func_FuncArg, variableMap map[string]*vispb.Vis_Variable) {
	for _, arg := range args {
		execFuncArg := &plannerpb.FuncToExecute_ArgValue{}
		execFuncArg.Name = arg.Name
		if arg.GetVariable() != "" {
			variable := variableMap[arg.GetVariable()]
			if variable.GetDefaultValue() != nil && variable.GetDefaultValue().GetValue() != "" {
				execFuncArg.Value = variable.GetDefaultValue().GetValue()
			} else if len(variable.ValidValues) > 0 {
				execFuncArg.Value = variable.ValidValues[0]
			} else {
				execFuncArg.Value = defaultForType(variable.Type)
			}
		} else if arg.GetValue() != "" {
			execFuncArg.Value = arg.GetValue()
		}
		execFunc.ArgValues = append(execFunc.ArgValues, execFuncArg)
	}
}

func visToExecFuncs(vis *vispb.Vis) []*plannerpb.FuncToExecute {
	funcs := make([]*plannerpb.FuncToExecute, 0)
	variableMap := make(map[string]*vispb.Vis_Variable)
	for _, v := range vis.Variables {
		variableMap[v.Name] = v
	}

	for _, f := range vis.GlobalFuncs {
		execFunc := &plannerpb.FuncToExecute{}
		execFunc.FuncName = f.Func.Name
		execFunc.OutputTablePrefix = f.Func.Name
		getArgsFromVis(execFunc, f.Func.Args, variableMap)
		funcs = append(funcs, execFunc)
	}

	for _, w := range vis.Widgets {
		if w.GetFunc() != nil {
			execFunc := &plannerpb.FuncToExecute{}
			execFunc.FuncName = w.GetFunc().Name
			execFunc.OutputTablePrefix = w.GetFunc().Name
			getArgsFromVis(execFunc, w.GetFunc().Args, variableMap)
			funcs = append(funcs, execFunc)
		}
	}
	return funcs
}

func scriptToQueryRequest(s *script) (*plannerpb.QueryRequest, error) {
	pxlFile, err := os.Open(s.pxlPath)
	if err != nil {
		return nil, err
	}
	defer pxlFile.Close()

	req := &plannerpb.QueryRequest{}
	b, err := io.ReadAll(pxlFile)
	if err != nil {
		return nil, err
	}

	req.QueryStr = string(b)
	if s.visPath == "" {
		return req, nil
	}

	visFile, err := os.Open(s.visPath)
	if err != nil {
		return nil, err
	}
	defer visFile.Close()

	vis := &vispb.Vis{}
	err = jsonpb.Unmarshal(visFile, vis)
	if err != nil {
		return nil, err
	}
	req.ExecFuncs = visToExecFuncs(vis)
	return req, nil
}

func loadSchemas() (*schemapb.Schema, error) {
	return godumpschemas.DumpSchemas()
}

func makePEMCarnotInfo(id uuid.UUID) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		HasDataStore:         true,
		HasGRPCServer:        false,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		AgentID:              utils.ProtoFromUUID(id),
		QueryBrokerAddress:   "pem" + id.String(),
	}
}

func makeKelvinCarnotInfo(id uuid.UUID) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		HasGRPCServer:        true,
		HasDataStore:         false,
		ProcessesData:        true,
		AcceptsRemoteSources: true,
		AgentID:              utils.ProtoFromUUID(id),
		QueryBrokerAddress:   "kelvin" + id.String(),
		GRPCAddress:          "1.1.1.1",
		SSLTargetName:        "ssl_targetname",
	}
}

func makeDistributedState(numPems int, numKelvins int) (*distributedpb.DistributedState, error) {
	ds := &distributedpb.DistributedState{}
	pemIDs := make([]*uuidpb.UUID, numPems)
	for i := 0; i < numPems; i++ {
		id, err := uuid.NewV4()
		if err != nil {
			return nil, err
		}
		pemIDs[i] = utils.ProtoFromUUID(id)
		ds.CarnotInfo = append(ds.CarnotInfo, makePEMCarnotInfo(id))
	}
	for i := 0; i < numKelvins; i++ {
		id, err := uuid.NewV4()
		if err != nil {
			return nil, err
		}
		ds.CarnotInfo = append(ds.CarnotInfo, makeKelvinCarnotInfo(id))
	}
	schemas, err := loadSchemas()
	if err != nil {
		return nil, err
	}
	for name, rel := range schemas.RelationMap {
		schemaInfo := &distributedpb.SchemaInfo{
			Name:      name,
			Relation:  rel,
			AgentList: pemIDs,
		}
		ds.SchemaInfo = append(ds.SchemaInfo, schemaInfo)
	}
	return ds, nil
}

func setupPlanner(req *plannerpb.QueryRequest, udfInfo *udfspb.UDFInfo, distributedState *distributedpb.DistributedState) (goplanner.GoPlanner, *distributedpb.LogicalPlannerState, error) {
	planner, err := goplanner.New(udfInfo)
	if err != nil {
		return planner, nil, err
	}
	flags, err := controllers.ParseQueryFlags(req.QueryStr)
	if err != nil {
		return planner, nil, err
	}
	planOpts := flags.GetPlanOptions()
	ps := &distributedpb.LogicalPlannerState{
		DistributedState:    distributedState,
		PlanOptions:         planOpts,
		ResultAddress:       "result_addr",
		ResultSSLTargetName: "result_ssl_targetname",
	}
	return planner, ps, nil
}

func loadUDFInfo() (*udfspb.UDFInfo, error) {
	udfInfo := &udfspb.UDFInfo{}
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if err != nil {
		return nil, err
	}
	err = proto.Unmarshal(b, udfInfo)
	if err != nil {
		return nil, err
	}
	return udfInfo, nil
}

func TestAllScriptsCompile(t *testing.T) {
	scripts, err := collectAllScripts()
	require.NoError(t, err)

	queryReqs := make([]*plannerpb.QueryRequest, len(scripts))
	for i, s := range scripts {
		req, err := scriptToQueryRequest(s)
		require.NoError(t, err)
		queryReqs[i] = req
	}
	// setup distributed planner state with 10 PEMs and 1 kelvin.
	distributedState, err := makeDistributedState(10, 1)
	require.NoError(t, err)

	// get udf info
	udfInfo, err := loadUDFInfo()
	require.NoError(t, err)

	for i, s := range scripts {
		t.Run(s.name, func(t *testing.T) {
			req := queryReqs[i]
			if strings.Contains(req.QueryStr, "pxtrace") {
				fmt.Println("skipping ", s.name, " because it contains a mutation")
				t.Skip(s.name)
			}
			planner, ps, err := setupPlanner(req, udfInfo, distributedState)
			require.NoError(t, err)
			defer planner.Free()
			req.LogicalPlannerState = ps
			result, err := planner.Plan(req)
			require.NoError(t, err)
			if result.Status != nil && result.Status.ErrCode != 0 {
				if result.Status.Context != nil {
					errContext := &compilerpb.CompilerErrorGroup{}
					err := types.UnmarshalAny(result.Status.Context, errContext)
					require.NoError(t, err)
					for _, e := range errContext.Errors {
						if e.GetLineColError() != nil {
							lineCol := e.GetLineColError()
							t.Logf("Planner error at %d:%d  %s", lineCol.Line, lineCol.Column, lineCol.Message)
						}
					}
				}
				t.Log(result.Status.Msg)
				t.Fail()
			}
		})
	}
}

var preventOptimizationResult *distributedpb.LogicalPlannerResult

func BenchmarkAllScripts(b *testing.B) {
	scripts, err := collectAllScripts()
	require.NoError(b, err)

	queryReqs := make([]*plannerpb.QueryRequest, len(scripts))
	for i, s := range scripts {
		req, err := scriptToQueryRequest(s)
		require.NoError(b, err)
		queryReqs[i] = req
	}
	// setup distributed planner state with 10 PEMs and 1 kelvin.
	distributedState, err := makeDistributedState(10, 1)
	require.NoError(b, err)

	// get udf info
	udfInfo, err := loadUDFInfo()
	require.NoError(b, err)

	for i, s := range scripts {
		b.Run(s.name, func(b *testing.B) {
			b.StopTimer()
			req := queryReqs[i]
			if strings.Contains(req.QueryStr, "pxtrace") {
				b.Skip(s.name)
			}
			var result *distributedpb.LogicalPlannerResult
			for i := 0; i < b.N; i++ {
				planner, ps, err := setupPlanner(req, udfInfo, distributedState)
				require.NoError(b, err)
				b.StartTimer()
				req.LogicalPlannerState = ps
				result, err = planner.Plan(req)
				b.StopTimer()
				require.NoError(b, err)
				planner.Free()
			}
			preventOptimizationResult = result
		})
	}
}
