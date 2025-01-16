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
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	CollectLogsCmd.Flags().StringP("namespace", "n", "", "The namespace vizier is deployed in")
}

// CollectLogsCmd is the "deploy" command.
var CollectLogsCmd = &cobra.Command{
	Use:   "collect-logs",
	Short: "Collect Pixie logs on the cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("namespace", cmd.Flags().Lookup("namespace"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		c := k8s.NewLogCollector()
		fName := fmt.Sprintf("pixie_logs_%s.zip", time.Now().Format("20060102150405"))
		err := c.CollectPixieLogs(fName)
		if err != nil {
			log.WithError(err).Fatal("Failed to get log files")
		}

		utils.Infof("Logs written to %s", fName)
	},
}
