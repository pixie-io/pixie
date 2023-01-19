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

package cmd

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"os"
	"strings"

	"github.com/fatih/color"
	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/api/ptproxy"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/script"
)

func init() {
	RunCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table|csv")
	RunCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	RunCmd.Flags().BoolP("list", "l", false, "List available scripts")
	RunCmd.Flags().BoolP("e2e_encryption", "e", true, "Enable E2E encryption")
	RunCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	RunCmd.Flags().StringP("cluster", "c", "", "ID of the cluster to run on. "+
		"Use 'px get viziers', or visit Admin console: work.withpixie.ai/admin, to find the ID")
	RunCmd.Flags().MarkHidden("all-clusters")

	RunCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")

	RunCmd.SetHelpFunc(func(command *cobra.Command, args []string) {
		viper.BindPFlag("bundle", command.Flags().Lookup("bundle"))
		br, err := createBundleReader()
		if err != nil {
			// Keep this as a log.Fatal() as opposed to using the utils, because it
			// is an unexpected error that Sentry should catch.
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

		fmt.Fprintf(os.Stderr, `
Script Usage:
  To show a script’s arguments and their descriptions:

    px run <script_name> -- --help
    px run px/namespace -- --help

  To pass an argument to a script:

    px run <script_name> -- --arg_name val
    px run px/namespace -- --namespace default

`)
	})
}

func createNewCobraCommand() *cobra.Command {
	return &cobra.Command{
		Use:   "run",
		Short: "Execute a script",
		PreRun: func(cmd *cobra.Command, args []string) {
			viper.BindPFlag("bundle", cmd.Flags().Lookup("bundle"))
		},
		Run: func(cmd *cobra.Command, args []string) {
			cloudAddr := viper.GetString("cloud_addr")
			format, _ := cmd.Flags().GetString("output")
			directVzAddr := viper.GetString("direct_vizier_addr")
			directVzKey := viper.GetString("direct_vizier_key")

			format = strings.ToLower(format)
			if format == "live" {
				LiveCmd.Run(cmd, args)
				return
			}

			listScripts, _ := cmd.Flags().GetBool("list")
			br, err := createBundleReader()
			if err != nil {
				// Keep this as a log.Fatal() as opposed to using the utils, because it
				// is an unexpected error that Sentry should catch.
				log.WithError(err).Fatal("Failed to read script bundle")
			}

			if listScripts {
				listBundleScripts(br, format)
				return
			}

			var execScript *script.ExecutableScript
			scriptFile, _ := cmd.Flags().GetString("file")
			var scriptArgs []string

			if scriptFile == "" {
				if len(args) == 0 {
					utils.Fatal("Expected script_name with script args.")
				}
				scriptName := args[0]
				execScript = br.MustGetScript(scriptName)
				scriptArgs = args[1:]
			} else {
				execScript, err = loadScriptFromFile(scriptFile)
				if err != nil {
					utils.WithError(err).Fatal("Failed to get query string")
				}
				scriptArgs = args
			}

			fs := execScript.GetFlagSet()
			if fs != nil {
				if err := fs.Parse(scriptArgs); err != nil {
					if err == flag.ErrHelp {
						os.Exit(0)
					}
					utils.WithError(err).Fatal("Failed to parse script flags")
				}
				err := execScript.UpdateFlags(fs)
				if err != nil {
					if errors.Is(err, script.ErrMissingRequiredArgument) {
						utils.Errorf("Missing required argument, please look at help below on how to pass in required arguments\n")
						cmd.Help()
						os.Exit(1)
					}
					utils.WithError(err).Fatal("Error parsing script flags")
				}
			}

			allClusters, _ := cmd.Flags().GetBool("all-clusters")
			selectedCluster, _ := cmd.Flags().GetString("cluster")
			clusterID := uuid.FromStringOrNil(selectedCluster)

			if !allClusters && clusterID == uuid.Nil && directVzAddr == "" {
				clusterID, err = vizier.GetCurrentVizier(cloudAddr)
				if err != nil {
					utils.WithError(err).Fatal("Could not fetch healthy vizier")
				}
			}

			conns := vizier.MustConnectVizier(cloudAddr, allClusters, clusterID, directVzAddr, directVzKey)
			useEncryption, _ := cmd.Flags().GetBool("e2e_encryption")
			if directVzAddr != "" {
				// There is no e2e encryption for direct mode.
				useEncryption = false
			}

			// Support Ctrl+C to cancel a query.
			ctx, cleanup := utils.WithSignalCancellable(context.Background())
			defer cleanup()
			err = vizier.RunScriptAndOutputResults(ctx, conns, execScript, format, useEncryption)

			if err != nil {
				vzErr, ok := err.(*vizier.ScriptExecutionError)
				switch {
				case ok && vzErr.Code() == vizier.CodeCanceled:
					utils.Info("Script was cancelled. Exiting.")
				case err == ptproxy.ErrNotAvailable:
					utils.WithError(err).Fatal("Cannot execute script")
				default:
					utils.WithError(err).Fatal("Failed to execute script")
				}
			}

			// Don't print cloudAddr live view link for direct mode.
			if directVzAddr != "" {
				return
			}

			// Get the name for this cluster for the live view
			var clusterName *string
			lister, err := vizier.NewLister(cloudAddr)
			if err != nil {
				log.WithError(err).Fatal("Failed to create Vizier lister")
			}
			vzInfo, err := lister.GetVizierInfo(clusterID)
			switch {
			case err != nil:
				utils.WithError(err).Errorf("Error getting cluster name for cluster %s", clusterID.String())
			case len(vzInfo) == 0:
				utils.Errorf("Error getting cluster name for cluster %s, no results returned", clusterID.String())
			default:
				clusterName = &(vzInfo[0].ClusterName)
			}

			if lvl := execScript.LiveViewLink(clusterName); lvl != "" {
				p := func(s string, a ...interface{}) {
					fmt.Fprintf(os.Stderr, s, a...)
				}
				b := color.New(color.Bold).Sprint
				u := color.New(color.Underline).Sprint
				p("\n%s %s: %s\n", color.CyanString("\n==> "),
					b("Live UI"), u(lvl))
			}
		},
	}
}

// RunCmd is the "query" command.
var RunCmd = createNewCobraCommand()

// RunSubCmd is the "query" command used as a subcommand with scripts.
var RunSubCmd = createNewCobraCommand()
