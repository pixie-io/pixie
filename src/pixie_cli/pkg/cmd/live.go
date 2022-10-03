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
	"flag"
	"os"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/live"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/script"
)

func init() {
	LiveCmd.Flags().StringP("bundle", "b", "", "Path/URL to bundle file")
	LiveCmd.Flags().StringP("file", "f", "", "Script file, specify - for STDIN")
	LiveCmd.Flags().BoolP("new_autocomplete", "n", false, "Whether to use the new autocomplete")
	LiveCmd.Flags().BoolP("e2e_encryption", "e", true, "Enable E2E encryption")

	LiveCmd.Flags().BoolP("all-clusters", "d", false, "Run script across all clusters")
	LiveCmd.Flags().StringP("cluster", "c", "", "Run only on selected cluster")
	LiveCmd.Flags().MarkHidden("all-clusters")
}

// LiveCmd is the "query" command.
var LiveCmd = &cobra.Command{
	Use:   "live",
	Short: "Interactive Pixie Views",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")

		useNewAC, _ := cmd.Flags().GetBool("new_autocomplete")

		br := mustCreateBundleReader()
		var execScript *script.ExecutableScript
		var err error
		scriptFile, _ := cmd.Flags().GetString("file")
		var scriptArgs []string

		if scriptFile == "" {
			if len(args) > 0 {
				scriptName := args[0]
				execScript = br.MustGetScript(scriptName)
				scriptArgs = args[1:]
			}
		} else {
			execScript, err = loadScriptFromFile(scriptFile)
			if err != nil {
				utils.WithError(err).Fatal("Failed to get query string")
			}
			scriptArgs = args
		}

		// `px live`, unlike `px run`, does not require a script to be passed in.
		// If a script is passed in, it will be executed. If it is not, then the user
		// will be prompted to select a script using ctrl+k.
		if execScript != nil {
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
					utils.WithError(err).Fatal("Error parsing script flags")
				}
			}
		}

		cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Could not connect to cloud")
		}
		aClient := cloudpb.NewAutocompleteServiceClient(cloudConn)
		allClusters, _ := cmd.Flags().GetBool("all-clusters")
		selectedCluster, _ := cmd.Flags().GetString("cluster")
		clusterUUID := uuid.FromStringOrNil(selectedCluster)
		if !allClusters && clusterUUID == uuid.Nil {
			clusterUUID, err = vizier.GetCurrentVizier(cloudAddr)
			if err != nil {
				utils.WithError(err).Fatal("Could not fetch healthy vizier")
			}
		}

		useEncryption, _ := cmd.Flags().GetBool("e2e_encryption")

		viziers := vizier.MustConnectHealthyDefaultVizier(cloudAddr, allClusters, clusterUUID)
		lv, err := live.New(br, viziers, cloudAddr, aClient, execScript, useNewAC, useEncryption, clusterUUID)
		if err != nil {
			utils.WithError(err).Fatal("Failed to initialize live view")
		}

		if err := lv.Run(); err != nil {
			utils.WithError(err).Fatal("Failed to run live view")
		}
	},
}
