package cmd

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json"

func init() {
	RunCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table")
	RunCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	RunCmd.Flags().BoolP("list", "l", false, "List available scripts")

	RunCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
	viper.BindPFlag("bundle", RunCmd.Flags().Lookup("bundle"))
}

// RunCmd is the "query" command.
var RunCmd = &cobra.Command{
	Use:   "run",
	Short: "Execute a script",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		listScripts, _ := cmd.Flags().GetBool("list")
		br, err := createBundleReader()
		if err != nil {
			log.WithError(err).Fatal("Failed to read script bundle")
		}

		if listScripts {
			listBundleScripts(br, format)
			return
		}

		var execScript *script.ExecutableScript
		scriptFile, _ := cmd.Flags().GetString("file")
		if scriptFile == "" {
			if len(args) != 1 {
				log.Fatal("Expected a single arg, script_name")
			}
			scriptName := args[0]
			execScript = br.MustGetScript(scriptName)
		} else {
			execScript, err = loadScriptFromFile(scriptFile)
			if err != nil {
				log.WithError(err).Fatal("Failed to get query string")
			}
		}

		// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Started",
			Properties: analytics.NewProperties().
				Set("scriptName", execScript.Metadata().ScriptName).
				Set("scriptString", execScript.ScriptString()),
		})

		v := mustConnectDefaultVizier(cloudAddr)

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()

		resp, err := v.ExecuteScriptStream(ctx, execScript.ScriptString())
		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Execution Failed",
				Properties: analytics.NewProperties().
					Set("scriptString", execScript.ScriptString()).
					Set("passthrough", v.PassthroughMode()),
			})
			log.WithError(err).Fatal("Failed to execute script")
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Success",
			Properties: analytics.NewProperties().
				Set("scriptString", execScript.ScriptString()).
				Set("outputFormat", format).
				Set("passthrough", v.PassthroughMode()),
		})

		tw := vizier.NewVizierStreamOutputAdapter(ctx, resp, format)
		tw.Finish()

		if execScript.Metadata().HasVis {
			p := func(s string, a ...interface{}) {
				fmt.Fprintf(os.Stderr, s, a...)
			}
			b := color.New(color.Bold).Sprint
			u := color.New(color.Underline).Sprintf
			p("\n%s %s: %s.\n", color.CyanString("\n==> "),
				b("Live UI"), u("https://%s/live?script=%s", cloudAddr, execScript.Metadata().ScriptName))
		}

	},
}
