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

	"px.dev/pixie/src/e2e_test/vizier/seq_tests/client/pkg/httpclient"
)

func init() {
	HTTPSeqCmd.PersistentFlags().IntP("start_sequence", "s", 0, "The start sequence number of the messages")
	HTTPSeqCmd.PersistentFlags().IntP("num_messages", "n", 10, "Number of messages to send")
	HTTPSeqCmd.PersistentFlags().IntP("num_conns", "c", 10, "Number of concurrent connections")
	HTTPSeqCmd.PersistentFlags().IntP("req_size", "r", 16, "The size of the request body")
	HTTPSeqCmd.PersistentFlags().IntP("resp_size", "z", 16, "The size of the response body")
	HTTPSeqCmd.PersistentFlags().Int("target_rps", 5000, "The requests per second to aim for")
}

// HTTPSeqCmd is the generates HTTP sequence messages.
var HTTPSeqCmd = &cobra.Command{
	Use:   "http",
	Short: "HTTP Sequence Load Test",
	Long:  "Run the HTTP/1.1 sequence load test",
	Run: func(cmd *cobra.Command, args []string) {
		addr, _ := cmd.Flags().GetString("addr")
		startSequence, _ := cmd.Flags().GetInt("start_sequence")
		numMessages, _ := cmd.Flags().GetInt("num_messages")
		numConns, _ := cmd.Flags().GetInt("num_conns")
		reqSize, _ := cmd.Flags().GetInt("req_size")
		respSize, _ := cmd.Flags().GetInt("resp_size")
		targetRPS, _ := cmd.Flags().GetInt("target_rps")
		log.
			WithField("addr", addr).
			WithField("start_sequence", startSequence).
			WithField("num_messages", numMessages).
			WithField("num_conns", numConns).
			WithField("req_size", reqSize).
			WithField("resp_size", respSize).
			WithField("target_rps", targetRPS).
			Info("Running HTTP sequence tests")

		c := httpclient.New(addr, startSequence, numMessages, numConns, reqSize, respSize, targetRPS)
		_ = c.Run()
		_ = c.PrintStats()
	},
}
