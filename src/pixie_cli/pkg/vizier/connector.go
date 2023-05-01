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
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vispb"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	cliUtils "px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/script"
)

const (
	dialTimeout = 5 * time.Second
	// We need an extra long retryTimeout because in passthrough mode the query broker
	// takes a while to receive the cancel message from the ptproxy.
	retryTimeout        = 30 * time.Second
	sleepBetweenRetries = 1 * time.Second
)

// Connector is an interface to Vizier.
type Connector struct {
	// The ID of the vizier.
	id           uuid.UUID
	conn         *grpc.ClientConn
	vz           vizierpb.VizierServiceClient
	vzDebug      vizierpb.VizierDebugServiceClient
	cloudAddr    string
	directVzAddr string
	directVzKey  string
}

// NewConnector returns a new connector.
func NewConnector(cloudAddr string, vzInfo *cloudpb.ClusterInfo, directVzAddr string, directVzKey string) (*Connector, error) {
	c := &Connector{}
	if vzInfo != nil {
		c.id = utils.UUIDFromProtoOrNil(vzInfo.ID)
	}
	c.cloudAddr = cloudAddr
	c.directVzAddr = directVzAddr
	c.directVzKey = directVzKey

	addr := cloudAddr
	if directVzAddr != "" {
		addr = directVzAddr
	}

	err := c.connect(addr)
	if err != nil {
		return nil, err
	}

	c.vz = vizierpb.NewVizierServiceClient(c.conn)
	c.vzDebug = vizierpb.NewVizierDebugServiceClient(c.conn)
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
	isInternal := strings.Contains(addr, "cluster.local")

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

func lookupVariable(variable string, computedArgs []script.Arg) (string, error) {
	for _, arg := range computedArgs {
		if arg.Name == variable {
			return arg.Value, nil
		}
	}
	return "", fmt.Errorf("variable '%s' not found", variable)
}

func makeFuncToExecute(f *vispb.Widget_Func, computedArgs []script.Arg, name string) (*vizierpb.ExecuteScriptRequest_FuncToExecute, error) {
	execFunc := &vizierpb.ExecuteScriptRequest_FuncToExecute{}
	execFunc.FuncName = f.Name

	execFunc.ArgValues = make([]*vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue, len(f.Args))
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

		execFunc.ArgValues[idx] = &vizierpb.ExecuteScriptRequest_FuncToExecute_ArgValue{
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
func GetFuncsToExecute(script *script.ExecutableScript) ([]*vizierpb.ExecuteScriptRequest_FuncToExecute, error) {
	if script.Vis == nil {
		return []*vizierpb.ExecuteScriptRequest_FuncToExecute{}, nil
	}
	// Accumulate the global function definitions.
	execFuncs := []*vizierpb.ExecuteScriptRequest_FuncToExecute{}
	computedArgs, err := script.ComputedArgs()
	if err != nil {
		return nil, err
	}

	if script.Vis.GlobalFuncs != nil {
		for _, f := range script.Vis.GlobalFuncs {
			execFunc, err := makeFuncToExecute(f.Func, computedArgs, f.OutputName)
			if err != nil {
				return []*vizierpb.ExecuteScriptRequest_FuncToExecute{}, err
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
			return []*vizierpb.ExecuteScriptRequest_FuncToExecute{}, err
		}
		execFuncs = append(execFuncs, execFunc)
	}
	return execFuncs, nil
}

func containsMutation(script *script.ExecutableScript) bool {
	r := regexp.MustCompile(`(?m:^(from pxtrace|import pxtrace)$)`)
	return len(r.FindAllStringSubmatch(script.ScriptString, -1)) > 0
}

func (c *Connector) restartConnAndResumeExecute(ctx context.Context, queryID string) (vizierpb.VizierService_ExecuteScriptClient, error) {
	err := c.connect(c.cloudAddr)
	if err != nil {
		return nil, err
	}

	reqPB := &vizierpb.ExecuteScriptRequest{
		QueryID:   queryID,
		ClusterID: c.id.String(),
	}
	return c.vz.ExecuteScript(ctx, reqPB)
}

func checkForTransientGRPCFailure(s *status.Status) bool {
	if s.Code() == codes.Unavailable {
		return true
	}
	if s.Code() == codes.Internal && strings.Contains(s.Message(), "RST_STREAM") {
		return true
	}
	return false
}

func checkForJWTExpired(s *status.Status) bool {
	return s.Code() == codes.Unauthenticated && strings.Contains(s.Message(), "invalid auth token")
}

type streamState struct {
	resp                vizierpb.VizierService_ExecuteScriptClient
	lastSuccessfulRetry time.Time
	results             chan *ExecData
	queryID             string
	firstErr            error
}

func (c *Connector) handleStream(ctx context.Context, state *streamState, first bool) bool {
	retry := true
	doNotRetry := false
	for {
		select {
		case <-state.resp.Context().Done():
			return doNotRetry
		case <-ctx.Done():
			return doNotRetry
		default:
			msg, err := state.resp.Recv()
			if err != nil {
				if s, ok := status.FromError(err); ok {
					if checkForTransientGRPCFailure(s) {
						if state.firstErr == nil {
							state.firstErr = s.Err()
						}
						return retry
					} else if checkForJWTExpired(s) {
						if state.firstErr == nil {
							state.firstErr = s.Err()
						}
						return retry
					}
				}
			}

			// This check is for backwards compatibility, and can be removed after our 2 week waiting period.
			if !first && msg != nil && msg.Status.GetMessage() == "Query should not be empty." {
				cliUtils.Errorf("Failed to resume query execution after transient error. Your version of Vizier does not support query resumption. Please update Vizier.")
				return doNotRetry
			}

			state.results <- &ExecData{ClusterID: c.id, Resp: msg, Err: err}
			if err != nil || msg == nil {
				return doNotRetry
			}
			if state.queryID == "" {
				state.queryID = msg.QueryID
			}
			state.lastSuccessfulRetry = time.Now()
			state.firstErr = nil
		}
	}
}

// ExecuteScriptStream execute a vizier query as a stream.
func (c *Connector) ExecuteScriptStream(ctx context.Context, script *script.ExecutableScript, encOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions) (chan *ExecData, error) {
	scriptStr := strings.TrimSpace(script.ScriptString)
	if len(scriptStr) == 0 {
		return nil, errors.New("input query is empty")
	}

	execFuncs, err := GetFuncsToExecute(script)
	if err != nil {
		return nil, err
	}

	scriptName := ""
	if !script.IsLocal {
		scriptName = script.ScriptName
	}
	reqPB := &vizierpb.ExecuteScriptRequest{
		QueryStr:          scriptStr,
		ClusterID:         c.id.String(),
		ExecFuncs:         execFuncs,
		Mutation:          containsMutation(script),
		EncryptionOptions: encOpts,
		QueryName:         scriptName,
	}

	if c.directVzAddr != "" {
		ctx = metadata.AppendToOutgoingContext(ctx, "X-DIRECT-VIZIER-KEY", c.directVzKey)
	} else {
		ctx = auth.CtxWithCreds(ctx)
	}

	resp, err := c.vz.ExecuteScript(ctx, reqPB)
	if err != nil {
		return nil, err
	}

	results := make(chan *ExecData)

	s := &streamState{
		resp:    resp,
		results: results,
	}

	go func() {
		defer close(results)
		shouldRetry := c.handleStream(ctx, s, true)
		for shouldRetry {
			// Wait some time between retries since the query might not be paused immediately on the query broker side after a failure.
			time.Sleep(sleepBetweenRetries)
			if !s.lastSuccessfulRetry.IsZero() && time.Since(s.lastSuccessfulRetry) > retryTimeout {
				if s.firstErr != nil {
					cliUtils.Errorf("Timedout trying to restart the connection after error: %s", s.firstErr.Error())
				} else {
					cliUtils.Errorf("Timedout trying to restart the connection.")
				}
				return
			}
			s.resp, err = c.restartConnAndResumeExecute(ctx, s.queryID)
			if err != nil {
				continue
			}
			shouldRetry = c.handleStream(ctx, s, false)
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
	reqPB := &vizierpb.DebugLogRequest{
		ClusterID: c.id.String(),
		PodName:   podName,
		Previous:  prev,
		Container: container,
	}
	ctx = auth.CtxWithCreds(ctx)
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
	ControlPlanePods []*vizierpb.VizierPodStatus
	DataPlanePods    []*vizierpb.VizierPodStatus
	Err              error
}

// DebugPodsRequest sends a debug pods request and returns data in a chan.
func (c *Connector) DebugPodsRequest(ctx context.Context) (chan *DebugPodsResponse, error) {
	reqPB := &vizierpb.DebugPodsRequest{
		ClusterID: c.id.String(),
	}
	ctx = auth.CtxWithCreds(ctx)
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
