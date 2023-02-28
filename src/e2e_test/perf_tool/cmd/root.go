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

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	levelStr := os.Getenv("LOG_LEVEL")
	level, err := log.ParseLevel(levelStr)
	if err != nil {
		level = log.InfoLevel
	}
	log.SetLevel(level)

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PX")
}

// RootCmd is the base command used to add other commands.
var RootCmd = &cobra.Command{
	Use:   "root",
	Short: "root command used by other commands",
}
