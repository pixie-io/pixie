package cmd

import (
	"context"
	"fmt"
	"os"
	"os/signal"
	"strings"

	"github.com/fatih/color"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/vizier"
)

func init() {
	RunCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table|csv")
	RunCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	RunCmd.Flags().BoolP("list", "l", false, "List available scripts")
	RunCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	RunCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
	RunCmd.Flags().MarkHidden("all-clusters")

	RunCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
	viper.BindPFlag("bundle", RunCmd.Flags().Lookup("bundle"))

	RunCmd.SetHelpFunc(func(command *cobra.Command, args []string) {
		br, err := createBundleReader()
		if err != nil {
			log.WithError(err).Fatal("Failed to read script bundle")
		}

		// Find the first valid script and dump out information about it.
		for idx, scriptName := range args {
			// First scriptName is command name.
			if idx == 0 {
				continue
			}
			execScript, err := br.GetScript(scriptName)
			if err != nil {
				// Not found.
				continue
			}

			fs := execScript.GetFlagSet()
			name := command.Name()
			flagsMarker := ""
			if fs != nil {
				flagsMarker = "-- [flags]"
			}
			fmt.Fprintf(os.Stderr, "Usage:\n  px %s %s %s\n", name, scriptName, flagsMarker)
			if fs != nil {
				fs.SetOutput(os.Stderr)
				fmt.Fprintf(os.Stderr, "\nFlags:\n")
				fs.Usage()
			}

			return
		}

		// If we get here, then just print the default usage.
		command.Usage()
	})
}

// RunCmd is the "query" command.
var RunCmd = &cobra.Command{
	Use:   "run",
	Short: "Execute a script",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")

		format = strings.ToLower(format)
		if format == "live" {
			LiveCmd.Run(cmd, args)
			return
		}

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
			if len(args) == 0 {
				log.Fatal("Expected script_name with script args.")
			}
			scriptName := args[0]
			execScript = br.MustGetScript(scriptName)
			fs := execScript.GetFlagSet()

			if fs != nil {
				if err := fs.Parse(args[1:]); err != nil {
					log.WithError(err).Fatal("Failed to parse script flags")
				}
				execScript.UpdateFlags(fs)
			}
		} else {
			execScript, err = loadScriptFromFile(scriptFile)
			if err != nil {
				log.WithError(err).Fatal("Failed to get query string")
			}
		}

		allClusters, _ := cmd.Flags().GetBool("all-clusters")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterID := uuid.FromStringOrNil(selectedCluster)

		if !allClusters || clusterID == uuid.Nil {
			clusterID, err = vizier.FirstHealthyVizier(cloudAddr)
			if err != nil {
				log.WithError(err).Fatal("Could not fetch healthy vizier")
			}
		}

		conns := vizier.MustConnectDefaultVizier(cloudAddr, allClusters, clusterID)

		// TODO(zasgar): Refactor this when we change to the new API to make analytics cleaner.
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Script Execution Started",
			Properties: analytics.NewProperties().
				Set("scriptName", execScript.ScriptName).
				Set("scriptString", execScript.ScriptString),
		})

		// Support Ctrl+C to cancel a query.
		ctx, cancel := context.WithCancel(context.Background())
		c := make(chan os.Signal, 1)
		signal.Notify(c, os.Interrupt)
		defer func() {
			signal.Stop(c)
			cancel()
		}()

		go func() {
			select {
			case <-c:
				cancel()
			case <-ctx.Done():
			}
		}()

		err = vizier.RunScriptAndOutputResults(ctx, conns, execScript, format)

		if err != nil {
			if vzErr, ok := err.(*vizier.ScriptExecutionError); ok && vzErr.Code() == vizier.CodeCanceled {
				log.Info("Script was cancelled. Exiting.")
			} else {
				log.WithError(err).Fatal("Failed to execute script")
			}
		}

		// Get the name for this cluster for the live view
		var clusterName *string
		lister, err := vizier.NewLister(cloudAddr)
		if err != nil {
			log.WithError(err).Fatal("Failed to create Vizier lister")
		}
		vzInfo, err := lister.GetVizierInfo(clusterID)
		if err != nil {
			log.WithError(err).Errorf("Error getting cluster name for cluster %s", clusterID.String())
		} else if len(vzInfo) == 0 {
			log.Errorf("Error getting cluster name for cluster %s, no results returned", clusterID.String())
		} else {
			clusterName = &(vzInfo[0].ClusterName)
		}

		if lvl := execScript.LiveViewLink(clusterName); lvl != "" {
			p := func(s string, a ...interface{}) {
				fmt.Fprintf(os.Stderr, s, a...)
			}
			b := color.New(color.Bold).Sprint
			u := color.New(color.Underline).Sprint
			p("\n%s %s: %s.\n", color.CyanString("\n==> "),
				b("Live UI"), u(lvl))
		}
	},
}
