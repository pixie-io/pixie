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

	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/script"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
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
	// Check for the presence of df.stream().
	// TODO(nserrino): Support addition of end_time as a way to execute a streaming query in this mode.
	if strings.Contains(execScript.ScriptString, "stream()") && format != "json" {
		return fmt.Errorf("Cannot execute a query containing df.stream() using px run with table output. " +
			"Please try using `px live` instead or setting output format to json (`-o json`).")
	}

	tw, err := runScript(ctx, conns, execScript, format)
	if err == nil { // Script ran successfully.
		tw.Finish()
		return nil
	}

	if tw == nil {
		return err
	}

	// Check if there is a pending mutation.
	mutationInfo, _ := tw.MutationInfo()
	if mutationInfo == nil || (mutationInfo.Status.Code != int32(codes.Unavailable)) {
		// There is no mutation in the script, or the mutation is complete.
		tw.Finish()
		return err
	}

	// Retry the mutation and use a jobrunner to show state.
	taskChs := make([]chan public_vizierapipb.LifeCycleState, len(mutationInfo.States))
	tasks := make([]utils.Task, len(mutationInfo.States))
	for i, mutation := range mutationInfo.States {
		tasks[i] = newTaskWrapper(fmt.Sprintf("Deploying %s", mutation.Name), func() error {
			for {
				select {
				case s, ok := <-taskChs[i]:
					if !ok || s == public_vizierapipb.FAILED_STATE {
						return errors.New("Could not deploy tracepoint")
					}
					if s == public_vizierapipb.RUNNING_STATE {
						return nil
					}
				}
			}
		})
		taskChs[i] = make(chan public_vizierapipb.LifeCycleState, 10)
	}

	schemaCh := make(chan bool, 10)
	tasks = append(tasks, newTaskWrapper("Preparing schema", func() error {
		for {
			select {
			case s, ok := <-schemaCh:
				if !ok {
					return errors.New("Could not prepare schema")
				}
				if s {
					return nil
				}
			}
		}
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

	vzJr.RunAndMonitor()
	if tw != nil {
		tw.Finish()
	}
	return err
}

func runScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript, format string) (*VizierStreamOutputAdapter, error) {
	resp, err := RunScript(ctx, conns, execScript)
	if err != nil {
		return nil, err
	}

	tw := NewVizierStreamOutputAdapter(ctx, resp, format)
	err = tw.WaitForCompletion()
	return tw, err
}

// RunScript runs the script and return the data channel
func RunScript(ctx context.Context, conns []*Connector, execScript *script.ExecutableScript) (chan *VizierExecData, error) {
	// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Script Execution Started",
		Properties: analytics.NewProperties().
			Set("scriptName", execScript.ScriptName).
			Set("scriptString", execScript.ScriptString),
	})

	mergedResponses := make(chan *VizierExecData)
	var eg errgroup.Group
	for _, conn := range conns {
		conn := conn
		eg.Go(func() error {
			resp, err := conn.ExecuteScriptStream(ctx, execScript)
			if err != nil {
				return err
			}

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
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Success",
			Properties: analytics.NewProperties().
				Set("scriptString", execScript.ScriptString),
		})
	}()
	return mergedResponses, nil
}
