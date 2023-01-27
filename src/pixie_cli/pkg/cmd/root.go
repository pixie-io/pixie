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
	"strings"

	"github.com/fatih/color"
	"github.com/segmentio/analytics-go/v3"
	"github.com/spf13/cobra"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/update"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
)

func init() {
	// Flags that are relevant to all sub-commands.
	RootCmd.PersistentFlags().StringP("cloud_addr", "a", "withpixie.ai:443", "The address of Pixie Cloud")
	viper.BindPFlag("cloud_addr", RootCmd.PersistentFlags().Lookup("cloud_addr"))

	RootCmd.PersistentFlags().StringP("dev_cloud_namespace", "m", "", "The namespace of Pixie Cloud, if using a cluster local cloud.")
	viper.BindPFlag("dev_cloud_namespace", RootCmd.PersistentFlags().Lookup("dev_cloud_namespace"))

	RootCmd.PersistentFlags().BoolP("y", "y", false, "Whether to accept all user input")
	viper.BindPFlag("y", RootCmd.PersistentFlags().Lookup("y"))

	RootCmd.PersistentFlags().BoolP("quiet", "q", false, "quiet mode")
	viper.BindPFlag("quiet", RootCmd.PersistentFlags().Lookup("quiet"))

	RootCmd.PersistentFlags().Bool("do_not_track", false, "do_not_track")
	viper.BindPFlag("do_not_track", RootCmd.PersistentFlags().Lookup("do_not_track"))

	RootCmd.PersistentFlags().String("direct_vizier_addr", "", "If set, connect directly to the Vizier service at the given address.")
	viper.BindPFlag("direct_vizier_addr", RootCmd.PersistentFlags().Lookup("direct_vizier_addr"))

	RootCmd.PersistentFlags().String("direct_vizier_key", "", "Should be set if direct_vizier_addr is set, the key to authenticate whether the user has permissions to connect to the Vizier service.")
	viper.BindPFlag("direct_vizier_key", RootCmd.PersistentFlags().Lookup("direct_vizier_key"))

	RootCmd.AddCommand(VersionCmd)
	RootCmd.AddCommand(AuthCmd)
	RootCmd.AddCommand(CollectLogsCmd)
	RootCmd.AddCommand(CreateCloudCertsCmd)
	RootCmd.AddCommand(DemoCmd)
	RootCmd.AddCommand(DeployCmd)
	RootCmd.AddCommand(DeleteCmd)
	RootCmd.AddCommand(UpdateCmd)
	RootCmd.AddCommand(RunCmd)
	RootCmd.AddCommand(LiveCmd)
	RootCmd.AddCommand(GetCmd)
	RootCmd.AddCommand(ScriptCmd)
	RootCmd.AddCommand(CreateBundle)
	RootCmd.AddCommand(DeployKeyCmd)
	RootCmd.AddCommand(APIKeyCmd)
	RootCmd.AddCommand(DebugCmd)

	RootCmd.PersistentFlags().MarkHidden("cloud_addr")
	RootCmd.PersistentFlags().MarkHidden("dev_cloud_namespace")
	RootCmd.PersistentFlags().MarkHidden("do_not_track")

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PX")

	// Maintain compatibility with old `PL` prefixed env names.
	// This will eventually be removed
	viper.BindEnv("cloud_addr", "PX_CLOUD_ADDR", "PL_CLOUD_ADDR")
	viper.BindEnv("testing_env", "PX_TESTING_ENV", "PL_TESTING_ENV")
	viper.BindEnv("cli_version", "PX_CLI_VERSION", "PL_CLI_VERSION")
	viper.BindEnv("vizier_version", "PX_VIZIER_VERSION", "PL_VIZIER_VERSION")
	viper.BindEnv("direct_vizier_key", "PX_DIRECT_VIZIER_KEY")
	viper.BindEnv("direct_vizier_addr", "PX_DIRECT_VIZIER_ADDR")

	viper.BindPFlags(pflag.CommandLine)

	// Usually flag parsing happens as a part of Cmd.Execute in Cobra.
	// However some of our CLI code relies on accessing flag data
	// before execute is called. So we manually pre-parse flags early.
	_ = RootCmd.ParseFlags(os.Args[1:])
}

func printEnvVars() {
	envs := os.Environ()
	var pxEnvs []string
	for _, env := range envs {
		if strings.HasPrefix(env, "PL_") || strings.HasPrefix(env, "PX_") {
			pxEnvs = append(pxEnvs, env)
		}
	}
	if len(pxEnvs) == 0 {
		return
	}
	green := color.New(color.Bold, color.FgGreen)
	green.Fprintf(os.Stderr, "*******************************\n")
	green.Fprintf(os.Stderr, "* ENV VARS\n")
	for _, env := range pxEnvs {
		green.Fprintf(os.Stderr, "* \t %s\n", env)
	}
	green.Fprintf(os.Stderr, "*******************************\n")
}

// RootCmd is the base command for Cobra.
var RootCmd = &cobra.Command{
	Use:   "px",
	Short: "Pixie CLI",
	// TODO(zasgar): Add description and update this.
	Long: `The Pixie command line interface.`,
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		printEnvVars()

		cloudAddr := viper.GetString("cloud_addr")
		if matched, err := regexp.MatchString(".+:[0-9]+$", cloudAddr); !matched && err == nil {
			viper.Set("cloud_addr", cloudAddr+":443")
		}

		if viper.IsSet("testing_env") && !viper.IsSet("dev_cloud_namespace") {
			// Setting this to the most likely default if not already set.
			viper.Set("dev_cloud_namespace", "plc-dev")
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

		// If the command requires auth, check that the user is logged in before running the command. Most of these commands,
		// such as `px deploy` run through most of the command before suddenly complaining partway through when we
		// actually hit Pixie Cloud.

		// Check if the subcommand requires auth.
		checkAuthForCmd(cmd)
		// Check if any parents of the subcommand requires auth.
		cmd.VisitParents(checkAuthForCmd)
	},
}

func checkAuthForCmd(c *cobra.Command) {
	if viper.GetString("direct_vizier_addr") != "" {
		if viper.GetString("direct_vizier_key") == "" {
			utils.Errorf("Failed to authenticate. `direct_vizier_key` must be provided using `PX_DIRECT_VIZIER_KEY`")
			os.Exit(1)
		}
		switch c {
		case DeployCmd, UpdateCmd, GetCmd, DeployKeyCmd, APIKeyCmd:
			utils.Errorf("These commands are unsupported in Direct Vizier mode.")
			os.Exit(1)
		default:
		}
		return
	}

	switch c {
	case DeployCmd, UpdateCmd, RunCmd, LiveCmd, GetCmd, ScriptCmd, DeployKeyCmd, APIKeyCmd:
		authenticated := auth.IsAuthenticated(viper.GetString("cloud_addr"))
		if !authenticated {
			utils.Errorf("Failed to authenticate. Please retry `px auth login`.")
			os.Exit(1)
		}
	default:
	}
}

// Execute is the main function for the Cobra CLI.
func Execute() {
	if err := RootCmd.Execute(); err != nil {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Exec Error",
		})
		utils.WithError(err).Fatal("Error executing command")
	}
}
