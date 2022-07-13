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
	"fmt"
	"os"

	"github.com/alecthomas/chroma/quick"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	ScriptCmd.AddCommand(ScriptListCmd)
	ScriptCmd.AddCommand(ScriptShowCmd)
	// Allow run as an alias to keep scripts self contained.
	ScriptCmd.AddCommand(RunSubCmd)

	ScriptCmd.PersistentFlags().StringP("bundle", "b", "", "Path/URL to bundle file")
	ScriptListCmd.Flags().StringP("output", "o", "", "Output format: one of: json|table")
}

// ScriptCmd is the "script" command.
var ScriptCmd = &cobra.Command{
	Use:     "script",
	Short:   "Get information about pre-registered scripts",
	Aliases: []string{"scripts"},
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("bundle", cmd.PersistentFlags().Lookup("bundle"))
	},
}

// ScriptListCmd is the "script list" command.
var ScriptListCmd = &cobra.Command{
	Use:     "list",
	Short:   "List pre-registered pxl scripts",
	Aliases: []string{"scripts"},
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("output_format", cmd.Flags().Lookup("output"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		br := mustCreateBundleReader()
		listBundleScripts(br, viper.GetString("output_format"))
	},
}

// ScriptShowCmd is the "script show" command.
var ScriptShowCmd = &cobra.Command{
	Use:     "show",
	Short:   "Dumps out the string for a particular pxl script",
	Args:    cobra.ExactArgs(1),
	Aliases: []string{"scripts"},
	Run: func(cmd *cobra.Command, args []string) {
		br := mustCreateBundleReader()
		scriptName := args[0]
		execScript := br.MustGetScript(scriptName)
		err := quick.Highlight(os.Stdout, execScript.ScriptString, "python3", "terminal16m", "monokai")
		if err != nil {
			fmt.Fprint(os.Stdout, execScript.ScriptString)
		}
	},
}
