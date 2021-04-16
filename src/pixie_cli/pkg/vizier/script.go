package vizier

import (
	"context"
	"errors"
	"fmt"
	"io"
	"strings"
	"time"

	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc/codes"
	"gopkg.in/segmentio/analytics-go.v3"

	public_vizierapipb "px.dev/pixie/src/api/public/vizierapipb"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/script"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
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
func RunScriptAndOutputResults(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, format string) error {
	// Check for the presence of df.stream() in the query.
	if strings.Contains(execScript.ScriptString, "stream()") && format != "json" {
		return fmt.Errorf("Cannot execute a query containing df.stream() using px run with table output. " +
			"Please try using `px live` instead or setting output format to json (`-o json`).")
	}

	tw, err := runScript(ctx, conns, execScript, format)
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
	taskChs := make([]chan public_vizierapipb.LifeCycleState, len(mutationInfo.States))
	tasks := make([]utils.Task, len(mutationInfo.States))
	for i, mutation := range mutationInfo.States {
		tasks[i] = newTaskWrapper(fmt.Sprintf("Deploying %s", mutation.Name), func() error {
			for s := range taskChs[i] {
				if s == public_vizierapipb.FAILED_STATE {
					return errors.New("Could not deploy tracepoint")
				}
				if s == public_vizierapipb.RUNNING_STATE {
					return nil
				}
			}
			// Channel was closed and we never saw a running state.
			return errors.New("Could not deploy tracepoint")
		})
		taskChs[i] = make(chan public_vizierapipb.LifeCycleState, 10)
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
			tw, err = runScript(ctx, conns, execScript, format)
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

func runScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, format string) (*StreamOutputAdapter, error) {
	resp, err := RunScript(ctx, conns, execScript)
	if err != nil {
		return nil, err
	}

	tw := NewStreamOutputAdapter(ctx, resp, format)
	err = tw.WaitForCompletion()
	return tw, err
}

// RunScript runs the script and return the data channel
func RunScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript) (chan *ExecData, error) {
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
		resp, err := conn.ExecuteScriptStream(ctx, execScript)
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
