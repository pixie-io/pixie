package vizier

import (
	"context"

	"gopkg.in/segmentio/analytics-go.v3"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

// RunScriptAndOutputResults runs the specified script on vizier and outputs based on format string.
func RunScriptAndOutputResults(ctx context.Context, v *Connector, execScript *script.ExecutableScript, format string) error {
	resp, err := RunScript(ctx, v, execScript)
	if err != nil {
		return err
	}
	tw := NewVizierStreamOutputAdapter(ctx, resp, format)
	tw.Finish()
	return nil
}

// RunScript runs the script and return the data channel
func RunScript(ctx context.Context, v *Connector, execScript *script.ExecutableScript) (chan *VizierExecData, error) {
	// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Script Execution Started",
		Properties: analytics.NewProperties().
			Set("scriptName", execScript.Metadata().ScriptName).
			Set("scriptString", execScript.ScriptString()),
	})

	resp, err := v.ExecuteScriptStream(ctx, execScript.ScriptString())
	if err != nil {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Failed",
			Properties: analytics.NewProperties().
				Set("scriptString", execScript.ScriptString()).
				Set("passthrough", v.PassthroughMode()),
		})
		return nil, err
	}

	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Script Execution Success",
		Properties: analytics.NewProperties().
			Set("scriptString", execScript.ScriptString()).
			Set("passthrough", v.PassthroughMode()),
	})
	return resp, nil
}
