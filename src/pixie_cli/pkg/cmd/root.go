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
	"os"
	"regexp"

	"github.com/fatih/color"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"

	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/update"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
)

func init() {
	// Flags that are relevant to all sub-commands.
	RootCmd.PersistentFlags().StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	viper.BindPFlag("cloud_addr", RootCmd.PersistentFlags().Lookup("cloud_addr"))

	RootCmd.PersistentFlags().BoolP("y", "y", false, "Whether to accept all user input")
	viper.BindPFlag("y", RootCmd.PersistentFlags().Lookup("y"))

	RootCmd.PersistentFlags().BoolP("quiet", "q", false, "quiet mode")
	viper.BindPFlag("quiet", RootCmd.PersistentFlags().Lookup("quiet"))

	RootCmd.AddCommand(VersionCmd)
	RootCmd.AddCommand(AuthCmd)
	RootCmd.AddCommand(CollectLogsCmd)
	RootCmd.AddCommand(CreateCloudCertsCmd)
	RootCmd.AddCommand(DemoCmd)
	RootCmd.AddCommand(DeployCmd)
	RootCmd.AddCommand(DeleteCmd)
	RootCmd.AddCommand(LoadClusterSecretsCmd)
	RootCmd.AddCommand(UpdateCmd)
	RootCmd.AddCommand(ProxyCmd)
	RootCmd.AddCommand(RunCmd)
	RootCmd.AddCommand(LiveCmd)
	RootCmd.AddCommand(GetCmd)
	RootCmd.AddCommand(ConfigCmd)
	RootCmd.AddCommand(ScriptCmd)

	RootCmd.AddCommand(CreateBundle)

	RootCmd.AddCommand(DeployKeyCmd)
	RootCmd.AddCommand(APIKeyCmd)

	RootCmd.AddCommand(DebugCmd)
	// Super secret flags for Pixies.
	RootCmd.PersistentFlags().MarkHidden("cloud_addr")
}

// nolint:errcheck
func printTestingBanner() {
	r := color.New(color.Bold, color.FgRed).Fprintf
	r(os.Stderr, "*******************************\n")
	r(os.Stderr, "* IN TESTING MODE\n")
	r(os.Stderr, "* \t PL_TESTING_ENV=%s\n", os.Getenv("PL_TESTING_ENV"))
	r(os.Stderr, "* \t PL_VIZIER_VERSION=%s\n", os.Getenv("PL_VIZIER_VERSION"))
	r(os.Stderr, "* \t PL_CLI_VERSION=%s\n", os.Getenv("PL_CLI_VERSION"))
	r(os.Stderr, "* \t PL_CLOUD_ADDR=%s\n", os.Getenv("PL_CLOUD_ADDR"))
	r(os.Stderr, "*******************************\n")
}

// RootCmd is the base command for Cobra.
var RootCmd = &cobra.Command{
	Use:   "px",
	Short: "Pixie CLI",
	// TODO(zasgar): Add description and update this.
	Long: `The Pixie command line interface.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		if matched, err := regexp.MatchString(".+:[0-9]+$", cloudAddr); !matched && err == nil {
			viper.Set("cloud_addr", cloudAddr+":443")
		}

		if e, has := os.LookupEnv("PL_TESTING_ENV"); has {
			printTestingBanner()
			if e == "dev" {
				// Setting this to the most likely default if not already set.
				if viper.GetString("dev_cloud_namespace") == "" {
					viper.Set("dev_cloud_namespace", "plc-dev")
				}
			}
		}

		p := cmd

		if p != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Exec CMD",
				Properties: analytics.NewProperties().
					Set("cmd", p.Name()),
			})
		}

		for p != nil && p != UpdateCmd {
			p = p.Parent()
		}

		if p == UpdateCmd {
			return
		}
		versionStr := update.UpdatesAvailable(viper.GetString("cloud_addr"))
		if versionStr != "" {
			cmdName := "<NONE>"
			if p != nil {
				cmdName = p.Name()
			}

			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Update Available",
				Properties: analytics.NewProperties().
					Set("cmd", cmdName),
			})
			c := color.New(color.Bold, color.FgGreen)
			_, _ = c.Fprintf(os.Stderr, "Update to version \"%s\" available. Run \"px update cli\" to update.\n", versionStr)
		}
	},
}

// Execute is the main function for the Cobra CLI.
func Execute() {
	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	if err := RootCmd.Execute(); err != nil {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Exec Error",
		})
		utils.WithError(err).Error("Error executing command")
		os.Exit(1)
	}
}
