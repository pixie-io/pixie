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

package vizier

import (
	"context"
	"errors"
	"fmt"
	"io"
	"os"
	"os/signal"
	"regexp"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"google.golang.org/grpc"

	public_vizierapipb "px.dev/pixie/src/api/proto/vizierapipb"
	"px.dev/pixie/src/cloud/cloudapipb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/script"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/vispb"
	"px.dev/pixie/src/utils"
	pl_api_vizierpb "px.dev/pixie/src/vizier/vizierpb"
)

const (
	dialTimeout = 5 * time.Second
)

// Connector is an interface to Vizier.
type Connector struct {
	// The ID of the vizier.
	id                 uuid.UUID
	conn               *grpc.ClientConn
	vz                 public_vizierapipb.VizierServiceClient
	vzDebug            pl_api_vizierpb.VizierDebugServiceClient
	vzToken            string
	passthroughEnabled bool
}

// NewConnector returns a new connector.
func NewConnector(cloudAddr string, vzInfo *cloudapipb.ClusterInfo, conn *ConnectionInfo) (*Connector, error) {
	c := &Connector{
		id: utils.UUIDFromProtoOrNil(vzInfo.ID),
	}

	if vzInfo.Config != nil {
		c.passthroughEnabled = vzInfo.Config.PassthroughEnabled
	}

	var err error
	if !c.passthroughEnabled {
		// We need to store the token to talk to Vizier directly.
		c.vzToken = conn.Token
		if conn.URL == nil {
			return nil, errors.New("missing Vizier URL, likely still initializing")
		}
		err = c.connect(conn.URL.Host)
	} else {
		err = c.connect(cloudAddr)
	}

	if err != nil {
		return nil, err
	}

	c.vz = public_vizierapipb.NewVizierServiceClient(c.conn)
	c.vzDebug = pl_api_vizierpb.NewVizierDebugServiceClient(c.conn)

	return c, nil
}

// Connect connects to Vizier (blocking)
func (c *Connector) connect(addr string) error {
	ctx, cancel := context.WithTimeout(context.Background(), dialTimeout)
	defer cancel()

	// Cancel dial on ctrl-c. Otherwise, it just hangs.
	ch := make(chan os.Signal, 1)
	signal.Notify(ch, os.Interrupt)
	go func() {
		<-ch
		cancel()
	}()
	isInternal := strings.ContainsAny(addr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return err
	}

	dialOpts = append(dialOpts, grpc.WithBlock())
	// Try to dial with a time out (ctrl-c can be used to cancel)
	conn, err := grpc.DialContext(ctx, addr, dialOpts...)
	if err != nil {
		return err
	}
	c.conn = conn

	return nil
}

// PassthroughMode returns true if passthrough mode is enabled.
func (c *Connector) PassthroughMode() bool {
	return c.passthroughEnabled
}

func lookupVariable(variable string, computedArgs []script.Arg) (string, error) {
	for _, arg := range computedArgs {
		if arg.Name == variable {
			return arg.Value, nil
		}
	}
	return "", fmt.Errorf("variable '%s' not found", variable)
}

func makeFuncToExecute(f *vispb.Widget_Func, computedArgs []script.Arg, name string) (*public_vizierapipb.ExecuteScriptRequest_FuncToExecute, error) {
	execFunc := &public_vizierapipb.ExecuteScriptRequest_FuncToExecute{}
	execFunc.FuncName = f.Name

	execFunc.ArgValues = make([]*public_vizierapipb.ExecuteScriptRequest_FuncToExecute_ArgValue, len(f.Args))
	for idx, arg := range f.Args {
		var value string
		var err error
		switch x := arg.Input.(type) {
		case *vispb.Widget_Func_FuncArg_Value:
			value = x.Value
		case *vispb.Widget_Func_FuncArg_Variable:
			// Lookup variable.
			value, err = lookupVariable(x.Variable, computedArgs)
			if err != nil {
				return nil, err
			}
		default:
			return nil, fmt.Errorf("Value not found")
		}

		execFunc.ArgValues[idx] = &public_vizierapipb.ExecuteScriptRequest_FuncToExecute_ArgValue{
			Name:  arg.Name,
			Value: value,
		}
	}

	execFunc.OutputTablePrefix = "widget"
	if name != "" {
		execFunc.OutputTablePrefix = name
	}

	return execFunc, nil
}

// GetFuncsToExecute extracts the funcs to execute from the script.
func GetFuncsToExecute(script *script.ExecutableScript) ([]*public_vizierapipb.ExecuteScriptRequest_FuncToExecute, error) {
	if script.Vis == nil {
		return []*public_vizierapipb.ExecuteScriptRequest_FuncToExecute{}, nil
	}
	// Accumulate the global function definitions.
	execFuncs := []*public_vizierapipb.ExecuteScriptRequest_FuncToExecute{}
	computedArgs, err := script.ComputedArgs()
	if err != nil {
		return nil, err
	}

	if script.Vis.GlobalFuncs != nil {
		for _, f := range script.Vis.GlobalFuncs {
			execFunc, err := makeFuncToExecute(f.Func, computedArgs, f.OutputName)
			if err != nil {
				return []*public_vizierapipb.ExecuteScriptRequest_FuncToExecute{}, err
			}
			execFuncs = append(execFuncs, execFunc)
		}
	}
	// Find function definitions within widgets.
	for _, w := range script.Vis.Widgets {
		var f *vispb.Widget_Func
		switch x := w.FuncOrRef.(type) {
		case *vispb.Widget_Func_:
			f = x.Func
		default:
			// Skip if it's not a function definition.
			continue
		}

		execFunc, err := makeFuncToExecute(f, computedArgs, w.Name)
		if err != nil {
			return []*public_vizierapipb.ExecuteScriptRequest_FuncToExecute{}, err
		}
		execFuncs = append(execFuncs, execFunc)
	}
	return execFuncs, nil
}

func containsMutation(script *script.ExecutableScript) bool {
	r := regexp.MustCompile(`(?m:^(from pxtrace|import pxtrace)$)`)
	return len(r.FindAllStringSubmatch(script.ScriptString, -1)) > 0
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, script *script.ExecutableScript) (chan *ExecData, error) {
	scriptStr := strings.TrimSpace(script.ScriptString)
	if len(scriptStr) == 0 {
		return nil, errors.New("input query is empty")
	}

	execFuncs, err := GetFuncsToExecute(script)
	if err != nil {
		return nil, err
	}

	reqPB := &public_vizierapipb.ExecuteScriptRequest{
		QueryStr:  scriptStr,
		ClusterID: c.id.String(),
		ExecFuncs: execFuncs,
		Mutation:  containsMutation(script),
	}

	if c.passthroughEnabled {
		ctx = auth.CtxWithCreds(ctx)
	} else {
		ctx = ctxWithTokenCreds(ctx, c.vzToken)
	}

	resp, err := c.vz.ExecuteScript(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *ExecData)
	go func() {
		for {
			select {
			case <-resp.Context().Done():
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()
				results <- &ExecData{ClusterID: c.id, Resp: msg, Err: err}
				if err != nil || msg == nil {
					close(results)
					return
				}
			}
		}
	}()
	return results, nil
}

// DebugLogResponse contains information about debug logs.
type DebugLogResponse struct {
	Data string
	Err  error
}

// DebugLogRequest sends a debug log request and returns data in a chan.
func (c *Connector) DebugLogRequest(ctx context.Context, podName string, prev bool, container string) (chan *DebugLogResponse, error) {
	reqPB := &pl_api_vizierpb.DebugLogRequest{
		ClusterID: c.id.String(),
		PodName:   podName,
		Previous:  prev,
		Container: container,
	}
	if c.passthroughEnabled {
		ctx = auth.CtxWithCreds(ctx)
	} else {
		ctx = ctxWithTokenCreds(ctx, c.vzToken)
	}

	resp, err := c.vzDebug.DebugLog(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *DebugLogResponse)
	go func() {
		defer close(results)
		for {
			select {
			case <-resp.Context().Done():
				if resp.Context().Err() != nil {
					results <- &DebugLogResponse{
						Err: resp.Context().Err(),
					}
				}
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()

				if err != nil || msg == nil {
					if err != nil && err != io.EOF {
						results <- &DebugLogResponse{
							Err: err,
						}
					}
					return
				}
				results <- &DebugLogResponse{
					Data: msg.Data,
				}
			}
		}
	}()
	return results, nil
}

// DebugPodsResponse contains information about debug logs.
type DebugPodsResponse struct {
	ControlPlanePods []*pl_api_vizierpb.VizierPodStatus
	DataPlanePods    []*pl_api_vizierpb.VizierPodStatus
	Err              error
}

// DebugPodsRequest sends a debug pods request and returns data in a chan.
func (c *Connector) DebugPodsRequest(ctx context.Context) (chan *DebugPodsResponse, error) {
	reqPB := &pl_api_vizierpb.DebugPodsRequest{
		ClusterID: c.id.String(),
	}
	if c.passthroughEnabled {
		ctx = auth.CtxWithCreds(ctx)
	} else {
		ctx = ctxWithTokenCreds(ctx, c.vzToken)
	}

	resp, err := c.vzDebug.DebugPods(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *DebugPodsResponse)
	go func() {
		defer close(results)
		for {
			select {
			case <-resp.Context().Done():
				if resp.Context().Err() != nil {
					results <- &DebugPodsResponse{
						Err: resp.Context().Err(),
					}
				}
				return
			case <-ctx.Done():
				return
			default:
				msg, err := resp.Recv()
				if msg == nil || err == io.EOF {
					return
				}
				if err != nil {
					results <- &DebugPodsResponse{
						Err: err,
					}
					return
				}
				results <- &DebugPodsResponse{
					ControlPlanePods: msg.ControlPlanePods,
					DataPlanePods:    msg.DataPlanePods,
				}
			}
		}
	}()
	return results, nil
}
