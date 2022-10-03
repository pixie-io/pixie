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
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/script"
)

func init() {
	CreateBundle.Flags().StringArrayP("base", "b", []string{"px"},
		"The base path(s) to use. for creating script bundles")
	CreateBundle.Flags().StringArrayP("search_path", "s", []string{},
		"The paths to search for the pxl files")
	CreateBundle.MarkFlagRequired("search_path")
	CreateBundle.Flags().StringP("out", "o", "-", "The output file")
}

// CreateBundle is the 'create-bundle' command. It's used to create a script bundle that can be used by the UI/CLI.
// This is a temporary command until we have proper script persistence.
var CreateBundle = &cobra.Command{
	Use:   "create-bundle",
	Short: "Create a bundle for scripts",
	// Since this is an internal command we will hide it.
	Hidden: true,
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("base", cmd.Flags().Lookup("base"))
		viper.BindPFlag("search_path", cmd.Flags().Lookup("search_path"))
		viper.BindPFlag("out", cmd.Flags().Lookup("out"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		basePaths, _ := cmd.Flags().GetStringArray("base")
		searchPaths, _ := cmd.Flags().GetStringArray("search_path")

		out, _ := cmd.Flags().GetString("out")
		b := script.NewBundleWriter(searchPaths, basePaths)
		err := b.Write(out)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to create bundle")
		}
	},
}
