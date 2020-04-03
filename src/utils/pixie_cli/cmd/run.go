package cmd

import (
	"context"
	"errors"
	"fmt"
	"io/ioutil"
	"os"
	"strings"
	"time"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/scripts"
)

const defaultBundleFile = "https://storage.googleapis.com/pixie-prod-artifacts/script-bundles/bundle.json"

func init() {
	RunCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table")
	RunCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	RunCmd.Flags().BoolP("list", "l", false, "List available scripts")
	RunCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
}

func listBundleScripts(bundleFile string, format string) {
	r, err := scripts.NewBundleReader(bundleFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read bundle")
	}

	w := components.CreateStreamWriter(format, os.Stdout)
	defer w.Finish()
	w.SetHeader("script_list", []string{"Name", "Description"})
	for _, script := range r.GetScriptMetadata() {
		w.Write([]interface{}{script.ScriptName, script.ShortDoc})
	}
}

func getScriptFromBundle(bundleFile, scriptName string) (string, bool, error) {
	r, err := scripts.NewBundleReader(bundleFile)
	if err != nil {
		log.WithError(err).Fatal("Failed to read bundle")
	}
	return r.GetScript(scriptName)
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
		bundleFile, _ := cmd.Flags().GetString("bundle")
		if bundleFile == "" {
			bundleFile = defaultBundleFile
		}
		if listScripts {
			listBundleScripts(bundleFile, format)
			return
		}

		var scriptName string
		var script string
		var err error
		var hasVis bool
		scriptFile, _ := cmd.Flags().GetString("file")
		if scriptFile == "" {
			if len(args) != 1 {
				log.Fatal("Expected a single arg, script_name")
			}
			scriptName = args[0]

			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Load Preset",
				Properties: analytics.NewProperties().
					Set("scriptName", scriptName),
			})

			script, hasVis, err = getScriptFromBundle(bundleFile, scriptName)
			if err != nil {
				log.WithError(err).Fatal("Failed to load script")
			}
		} else {
			script, err = getScriptString(scriptFile)
			if err != nil {
				log.WithError(err).Fatal("Failed to get query string")
			}
		}

		// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Started",
			Properties: analytics.NewProperties().
				Set("scriptString", script),
		})

		v := mustConnectDefaultVizier(cloudAddr)

		if v.PassthroughMode() {
			fmt.Fprint(os.Stderr, "Connection Mode: passthrough\n")
		} else {
			fmt.Fprintf(os.Stderr, "Connection Mode: secure-direct\n")
		}

		ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
		defer cancel()

		resp, err := v.ExecuteScriptStream(ctx, script)

		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Script Execution Failed",
				Properties: analytics.NewProperties().
					Set("scriptString", script).
					Set("passthrough", v.PassthroughMode()),
			})
			log.WithError(err).Fatal("Failed to execute script")
		}

		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Success",
			Properties: analytics.NewProperties().
				Set("scriptString", script).
				Set("outputFormat", format).
				Set("passthrough", v.PassthroughMode()),
		})

		tw := vizier.NewVizierStreamOutputAdapter(ctx, resp, format)
		tw.Finish()

		if hasVis {
			p := func(s string, a ...interface{}) {
				fmt.Fprintf(os.Stderr, s, a...)
			}
			b := color.New(color.Bold).Sprint
			u := color.New(color.Underline).Sprintf
			p("\n%s %s: %s.\n", color.CyanString("\n==> "),
				b("Live UI"), u("https://%s/live?script=%s", cloudAddr, scriptName))
		}

	},
}

func getScriptString(path string) (string, error) {
	var qb []byte
	var err error
	if path == "-" {
		// Read from STDIN.
		qb, err = ioutil.ReadAll(os.Stdin)
		if err != nil {
			panic(err)
		}
	} else {
		r, err := os.Open(path)
		if err != nil {
			return "", err
		}

		qb, err = ioutil.ReadAll(r)
		if err != nil {
			return "", err
		}
	}

	if len(qb) == 0 {
		return "", errors.New("script string is empty")
	}
	return string(qb), nil
}
