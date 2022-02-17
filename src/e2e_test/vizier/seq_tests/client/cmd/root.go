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
	"crypto/tls"
	"net/http"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
)

func init() {
	RootCmd.PersistentFlags().StringP("addr", "a", "https://localhost:5001", "The address of the server with protocol")
	RootCmd.PersistentFlags().BoolP("skip_verify", "k", false, "Skip SSL verification (if SSL is used)")

	RootCmd.AddCommand(SeqCmd)
}

// RootCmd is the base command for this CLI.
var RootCmd = &cobra.Command{
	Use:   "ltc",
	Short: "Load Test Client",
	Long:  "This is the load test client to test Pixie under various conditions",
	PersistentPreRun: func(cmd *cobra.Command, args []string) {
		log.WithField("time", time.Now()).
			Info("Starting Load Test Client")
		if skip, _ := cmd.Flags().GetBool("skip_verify"); skip {
			http.DefaultTransport.(*http.Transport).TLSClientConfig = &tls.Config{InsecureSkipVerify: true}
		}
	},
}
