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
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/segmentio/analytics-go/v3"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"

	apiutils "px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/utils/script"
)

const (
	equalityThreshold          = 0.01
	headersInstalledPercColumn = "headers_installed_percent" // must match the column in px/agent_status_diagnostics
)

type taskWrapper struct {
	name string
	run  func() error
}

func newTaskWrapper(name string, run func() error) *taskWrapper {
	return &taskWrapper{
		name,
		run,
	}
}

func (t *taskWrapper) Name() string {
	return t.name
}

func (t *taskWrapper) Run() error {
	return t.run()
}

// RunScriptAndOutputResults runs the specified script on vizier and outputs based on format string.
func RunScriptAndOutputResults(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, format string, useEncryption bool) error {
	// Check for the presence of df.stream() in the query.
	if strings.Contains(execScript.ScriptString, "stream()") && format != "json" {
		return fmt.Errorf("Cannot execute a query containing df.stream() using px run with table output. " +
			"Please try using `px live` instead or setting output format to json (`-o json`).")
	}

	tw, err := runScript(ctx, conns, execScript, format, useEncryption)
	if err == nil { // Script ran successfully.
		err = tw.Finish()
		if err != nil {
			return err
		}
		return nil
	}

	if tw == nil {
		return err
	}

	// Check if there is a pending mutation.
	mutationInfo, _ := tw.MutationInfo()
	if mutationInfo == nil || (mutationInfo.Status.Code != int32(codes.Unavailable)) {
		// There is no mutation in the script, or the mutation is complete.
		err = tw.Finish()
		if err != nil {
			return err
		}
		return err
	}

	// Retry the mutation and use a jobrunner to show state.
	taskChs := make([]chan vizierpb.LifeCycleState, len(mutationInfo.States))
	tasks := make([]utils.Task, len(mutationInfo.States))
	for i, mutation := range mutationInfo.States {
		tasks[i] = newTaskWrapper(fmt.Sprintf("Deploying %s", mutation.Name), func() error {
			for s := range taskChs[i] {
				if s == vizierpb.FAILED_STATE {
					return errors.New("Could not deploy tracepoint")
				}
				if s == vizierpb.RUNNING_STATE {
					return nil
				}
			}
			// Channel was closed and we never saw a running state.
			return errors.New("Could not deploy tracepoint")
		})
		taskChs[i] = make(chan vizierpb.LifeCycleState, 10)
	}

	schemaCh := make(chan bool, 10)
	//The preallocated slice is filled by the for loop and then this adds one more element.
	//nolint:makezero
	tasks = append(tasks, newTaskWrapper("Preparing schema", func() error {
		for s := range schemaCh {
			if s {
				return nil
			}
		}
		return errors.New("Could not prepare schema")
	}))

	vzJr := utils.NewParallelTaskRunner(tasks)

	// Run retries.
	go func() {
		defer func() {
			for _, ch := range taskChs {
				close(ch)
			}
			close(schemaCh)
		}()

		tries := 5
		for tries > 0 {
			tw, err = runScript(ctx, conns, execScript, format, useEncryption)
			if err == nil {
				schemaCh <- true
				break
			}
			if tw == nil {
				break
			}

			// Check if there is a pending mutation.
			mutationInfo, _ = tw.MutationInfo()
			if mutationInfo == nil || (mutationInfo.Status.Code != int32(codes.Unavailable)) {
				schemaCh <- true
				break
			}

			// Update channels with new mutation state.
			for i, s := range mutationInfo.States {
				taskChs[i] <- s.State
			}
			schemaCh <- mutationInfo.Status.Code != int32(codes.Unavailable)

			time.Sleep(time.Second * 5)

			tries--
		}
	}()

	err = vzJr.RunAndMonitor()
	if err != nil {
		return err
	}
	if tw != nil {
		err = tw.Finish()
		if err != nil {
			return err
		}
	}
	return err
}

func runScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, format string, useEncryption bool) (*StreamOutputAdapter, error) {
	var encOpts, decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions
	var err error
	if useEncryption {
		encOpts, decOpts, err = apiutils.CreateEncryptionOptions()
		if err != nil {
			return nil, err
		}
	}

	resp, err := RunScript(ctx, conns, execScript, encOpts)
	if err != nil {
		return nil, err
	}

	tw := NewStreamOutputAdapter(ctx, resp, format, decOpts)
	err = tw.WaitForCompletion()
	return tw, err
}

func RunScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, encOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions) (chan *ExecData, error) {
	// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Script Execution Started",
		Properties: analytics.NewProperties().
			Set("scriptName", execScript.ScriptName).
			Set("scriptString", execScript.ScriptString),
	})

	mergedResponses := make(chan *ExecData)
	var eg errgroup.Group

	for _, conn := range conns {
		conn := conn
		resp, err := conn.ExecuteScriptStream(ctx, execScript, encOpts)
		if err != nil {
			// Collect this error for tracking.
			eg.Go(func() error {
				return err
			})
			return nil, err
		}

		eg.Go(func() error {
			for v := range resp {
				mergedResponses <- v
				if v.Err != nil && v.Err == io.EOF {
					return nil
				}
				if v.Err != nil {
					return v.Err
				}
			}
			return nil
		})
	}

	go func() {
		err := eg.Wait()
		close(mergedResponses)

		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Execution Failed",
				Properties: analytics.NewProperties().
					Set("scriptString", execScript.ScriptString),
			})
		} else {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Execution Success",
				Properties: analytics.NewProperties().
					Set("scriptString", execScript.ScriptString),
			})
		}
	}()
	return mergedResponses, nil
}

type HealthCheckWarning struct {
	message string
}

func (h *HealthCheckWarning) Error() string {
	return h.message
}

func newHealthCheckWarning(message string) error {
	return &HealthCheckWarning{message}
}

func evaluateHealthCheckResult(output string) error {
	jsonData := make(map[string]interface{})

	err := json.Unmarshal([]byte(output), &jsonData)
	if err != nil {
		return err
	}
	if v, ok := jsonData[headersInstalledPercColumn]; ok {
		switch t := v.(type) {
		case float64:
			if math.Abs(1.0-t) > equalityThreshold {
				msg := "Detected missing kernel headers on your cluster's nodes. This may cause issues with the Pixie agent. Please install kernel headers on all nodes."
				return newHealthCheckWarning(msg)
			}
		}
	} else {
		return newHealthCheckWarning("Unable to detect if the cluster's nodes have the distro kernel headers installed (vizier too old to perform this check). Please ensure that the kernel headers are installed on all nodes.")
	}
	return nil
}

type healthCheckData struct {
	line string
	err  error
}

// Runs the health check script on the specified vizier. The script's output is evaluated with
// the evaluateHealthCheckResult function to determine if the cluster is healthy. Only a single
// line of output will be parsed from the script.
func runHealthCheckScript(v *Connector, execScript *script.ExecutableScript) (chan string, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	var encOpts, decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions

	resp, err := RunScript(ctx, []*Connector{v}, execScript, encOpts)
	if err != nil {
		return nil, err
	}

	reader, writer := io.Pipe()
	factoryFunc := func(md *vizierpb.ExecuteScriptResponse_MetaData) components.OutputStreamWriter {
		return components.CreateStreamWriter("json", writer)
	}
	tw := NewStreamOutputAdapterWithFactory(ctx, resp, "json", decOpts, factoryFunc)

	bufReader := bufio.NewReader(reader)
	errCh := make(chan error, 1)
	streamCh := make(chan *healthCheckData)
	outputCh := make(chan string, 1)
	go func() {
		defer close(streamCh)
		defer reader.Close()
		for {
			line, err := bufReader.ReadString('\n')
			streamCh <- &healthCheckData{line, err}
			if err != nil {
				return
			}
		}
	}()
	// Consumes the output from the stream and returns the last item received unless
	// the context deadline is reached. The px/agent_status_diagnostics script only
	// outputs one line, but even when the fallback case is used (px/agent_status),
	// a single line informs whether the health check passed or failed.
	go func() {
		defer close(errCh)
		defer close(outputCh)
		var healthCheckErr error
		var lastLine string
		for {
			select {
			case <-ctx.Done():
				errCh <- ctx.Err()
				return
			case data := <-streamCh:
				// If data is nil or the error is io.EOF, the stream has been completely consumed (channel closed).
				if data == nil || errors.Is(data.err, io.EOF) {
					outputCh <- lastLine
					errCh <- healthCheckErr
					return
				}
				lastLine = data.line
				err := data.err
				if err == nil {
					healthCheckErr = evaluateHealthCheckResult(lastLine)
				}
			}
		}
	}()

	err = tw.Finish()
	writer.Close()

	if err != nil {
		return outputCh, err
	}
	err = <-errCh

	return outputCh, err
}

// RunSimpleHealthCheckScript runs a diagnostic pxl script to verify query serving works.
// For newer viziers, it performs additional checks to ensure that the cluster is healthy
// and that common issues are detected.
func RunSimpleHealthCheckScript(br *script.BundleManager, cloudAddr string, clusterID uuid.UUID) (chan string, error) {
	v, err := ConnectionToVizierByID(cloudAddr, clusterID)
	if err != nil {
		return nil, err
	}
	execScript, err := br.GetScript(script.AgentStatusDiagnosticsScript)
	if err != nil {
		execScript, err = br.GetScript(script.AgentStatusScript)
		if err != nil {
			return nil, err
		}
	}

	resp, err := runHealthCheckScript(v, execScript)
	if scriptErr, ok := err.(*ScriptExecutionError); ok {
		if scriptErr.Code() == CodeCompilerError {
			// If the script compilation failed, we fall back to the old health check script.
			execScript, err = br.GetScript(script.AgentStatusScript)
			if err != nil {
				return nil, err
			}
			return runHealthCheckScript(v, execScript)
		}
	}
	return resp, err
}
