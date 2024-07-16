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

	"github.com/mitchellh/mapstructure"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"golang.org/x/exp/slices"

	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
)

func init() {
	ConfigCmd.AddCommand(ConfigListCmd)
	ConfigCmd.AddCommand(ConfigSetCmd)

	ConfigSetCmd.Flags().StringArray("key", []string{}, "Key to set")
	ConfigSetCmd.Flags().StringArray("value", []string{}, "Value to set")
}

var settableConfigKeys = pxconfig.GetSettableConfigKeys()

var ConfigSetCmd = &cobra.Command{
	Use:   "set",
	Short: "Set configuration options",
	Run: func(cmd *cobra.Command, args []string) {
		keys, _ := cmd.Flags().GetStringArray("key")
		values, _ := cmd.Flags().GetStringArray("value")

		input := map[string]interface{}{}
		for i := 0; i < len(keys); i++ {
			key := keys[i]
			value := values[i]
			input[key] = value
		}

		cfg := pxconfig.Cfg()
		err := mapstructure.Decode(input, cfg)

		if err != nil {
			log.Fatalf("Failed to set config: %v", err)
		}

		err = pxconfig.UpdateConfig(cfg)
		if err != nil {
			log.Fatalf("Failed to update config: %v", err)
		}
	},
	PreRun: func(cmd *cobra.Command, args []string) {
		keys, _ := cmd.Flags().GetStringArray("key")
		values, _ := cmd.Flags().GetStringArray("value")

		if len(keys) != len(values) {
			log.Fatal("the number of --key and --value flags must match")
			os.Exit(1)
		}

		for _, key := range keys {
			if !slices.Contains(settableConfigKeys, key) {
				log.Fatalf("Key '%s' is not settable. Must be one of %v", key, settableConfigKeys)
			}
		}
	},
}

// ConfigListCmd is the "config list" command.
var ConfigListCmd = &cobra.Command{
	Use:   "list",
	Short: "List configuration options",
	Run: func(cmd *cobra.Command, args []string) {
		cfg := pxconfig.Cfg().ConfigInfoSettable

		var configuration map[string]interface{}
		err := mapstructure.Decode(cfg, &configuration)
		if err != nil {
			log.Fatalf("Failed to decode config: %v", err)
		}
		for key, value := range configuration {
			fmt.Printf("%s: %v\n", key, value)
		}
	},
}

// ConfigCmd is the "config" command.
var ConfigCmd = &cobra.Command{
	Use:   "config",
	Short: "Get information about the pixie config file",
}
