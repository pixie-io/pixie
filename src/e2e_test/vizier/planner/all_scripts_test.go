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
	"fmt"
	"io/ioutil"
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
	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/compilerpb"
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

func collectAllScripts(t *testing.T) []*script {
	dirToScript := make(map[string]*script)

	scriptDirAbsPath, err := bazel.Runfile(scriptDir)
	if err != nil {
		t.Fatal("Must run test through bazel, so that it can find the pxl scripts")
	}
	pxlPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*.pxl"))
	assert.Nil(t, err)
	otherPxlPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*", "*.pxl"))
	assert.Nil(t, err)
	pxlPaths = append(pxlPaths, otherPxlPaths...)
	for _, p := range pxlPaths {
		dirname := filepath.Dir(p)
		name := path.Join(filepath.Base(filepath.Dir(dirname)), filepath.Base(dirname))
		dirToScript[dirname] = &script{name: name, pxlPath: p}
	}
	visPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*.json"))
	assert.Nil(t, err)
	otherVisPaths, err := filepath.Glob(path.Join(scriptDirAbsPath, "*", "*", "*", "*.json"))
	assert.Nil(t, err)
	visPaths = append(visPaths, otherVisPaths...)
	for _, p := range visPaths {
		dirname := filepath.Dir(p)
		s, ok := dirToScript[dirname]
		if !ok {
			t.Fatalf("Found vis.json without pxl script: %s", p)
		}
		s.visPath = p
	}

	scripts := make([]*script, 0, len(dirToScript))
	for _, s := range dirToScript {
		scripts = append(scripts, s)
	}
	return scripts
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

func getArgsFromVis(t *testing.T, execFunc *plannerpb.FuncToExecute, args []*vispb.Widget_Func_FuncArg, variableMap map[string]*vispb.Vis_Variable) {
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

func visToExecFuncs(t *testing.T, vis *vispb.Vis) []*plannerpb.FuncToExecute {
	funcs := make([]*plannerpb.FuncToExecute, 0)
	variableMap := make(map[string]*vispb.Vis_Variable)
	for _, v := range vis.Variables {
		variableMap[v.Name] = v
	}

	for _, f := range vis.GlobalFuncs {
		execFunc := &plannerpb.FuncToExecute{}
		execFunc.FuncName = f.Func.Name
		execFunc.OutputTablePrefix = f.Func.Name
		getArgsFromVis(t, execFunc, f.Func.Args, variableMap)
		funcs = append(funcs, execFunc)
	}

	for _, w := range vis.Widgets {
		if w.GetFunc() != nil {
			execFunc := &plannerpb.FuncToExecute{}
			execFunc.FuncName = w.GetFunc().Name
			execFunc.OutputTablePrefix = w.GetFunc().Name
			getArgsFromVis(t, execFunc, w.GetFunc().Args, variableMap)
			funcs = append(funcs, execFunc)
		}
	}
	return funcs
}

func scriptToQueryRequest(t *testing.T, s *script) *plannerpb.QueryRequest {
	pxlFile, err := os.Open(s.pxlPath)
	assert.Nil(t, err)
	defer pxlFile.Close()

	req := &plannerpb.QueryRequest{}
	b, err := ioutil.ReadAll(pxlFile)
	assert.Nil(t, err)

	req.QueryStr = string(b)
	if s.visPath == "" {
		return req
	}

	visFile, err := os.Open(s.visPath)
	assert.Nil(t, err)
	defer visFile.Close()

	vis := &vispb.Vis{}
	err = jsonpb.Unmarshal(visFile, vis)
	assert.Nil(t, err)
	req.ExecFuncs = visToExecFuncs(t, vis)
	return req
}

func loadSchemas(t *testing.T) *schemapb.Schema {
	schema := &schemapb.Schema{}

	schemaPath, err := bazel.Runfile("src/e2e_test/vizier/planner/dump_schemas/all_schemas.bin")
	assert.Nil(t, err)
	schemaFile, err := os.Open(schemaPath)
	assert.Nil(t, err)

	b, err := ioutil.ReadAll(schemaFile)
	assert.Nil(t, err)

	err = proto.Unmarshal(b, schema)
	assert.Nil(t, err)
	return schema
}

func makePEMCarnotInfo(t *testing.T, id uuid.UUID) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		HasDataStore:         true,
		HasGRPCServer:        false,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		AgentID:              utils.ProtoFromUUID(id),
		QueryBrokerAddress:   "pem" + id.String(),
	}
}

func makeKelvinCarnotInfo(t *testing.T, id uuid.UUID) *distributedpb.CarnotInfo {
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

func makeDistributedState(t *testing.T, numPems int, numKelvins int) *distributedpb.DistributedState {
	ds := &distributedpb.DistributedState{}
	pemIDs := make([]*uuidpb.UUID, numPems)
	for i := 0; i < numPems; i++ {
		id, err := uuid.NewV4()
		assert.Nil(t, err)
		pemIDs[i] = utils.ProtoFromUUID(id)
		ds.CarnotInfo = append(ds.CarnotInfo, makePEMCarnotInfo(t, id))
	}
	for i := 0; i < numKelvins; i++ {
		id, err := uuid.NewV4()
		assert.Nil(t, err)
		ds.CarnotInfo = append(ds.CarnotInfo, makeKelvinCarnotInfo(t, id))
	}
	schemas := loadSchemas(t)
	for name, rel := range schemas.RelationMap {
		schemaInfo := &distributedpb.SchemaInfo{
			Name:      name,
			Relation:  rel,
			AgentList: pemIDs,
		}
		ds.SchemaInfo = append(ds.SchemaInfo, schemaInfo)
	}
	return ds
}

func TestAllScriptsCompile(t *testing.T) {
	scripts := collectAllScripts(t)
	queryReqs := make([]*plannerpb.QueryRequest, len(scripts))
	for i, s := range scripts {
		queryReqs[i] = scriptToQueryRequest(t, s)
	}
	// setup distributed planner state with 10 PEMs and 1 kelvin.
	distributedState := makeDistributedState(t, 10, 1)

	// get udf info
	udfInfo := &udfspb.UDFInfo{}
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	assert.Nil(t, err)
	err = proto.Unmarshal(b, udfInfo)
	assert.Nil(t, err)

	for i, s := range scripts {
		t.Run(s.name, func(t *testing.T) {
			req := queryReqs[i]
			if strings.Contains(req.QueryStr, "pxtrace") {
				fmt.Println("skipping ", s.name, " because it contains a mutation")
				t.Skip(s.name)
			}
			planner, err := goplanner.New(udfInfo)
			assert.Nil(t, err)
			flags, err := controllers.ParseQueryFlags(req.QueryStr)
			assert.Nil(t, err)
			planOpts := flags.GetPlanOptions()
			ps := &distributedpb.LogicalPlannerState{
				DistributedState:    distributedState,
				PlanOptions:         planOpts,
				ResultAddress:       "result_addr",
				ResultSSLTargetName: "result_ssl_targetname",
			}
			result, err := planner.Plan(ps, req)
			assert.Nil(t, err)
			if result.Status != nil && result.Status.ErrCode != 0 {
				errContext := &compilerpb.CompilerErrorGroup{}
				err := types.UnmarshalAny(result.Status.Context, errContext)
				assert.Nil(t, err)
				for _, e := range errContext.Errors {
					if e.GetLineColError() != nil {
						lineCol := e.GetLineColError()
						t.Logf("Planner error at %d:%d  %s", lineCol.Line, lineCol.Column, lineCol.Message)
					}
				}
				t.Fail()
			}
		})
	}
}
