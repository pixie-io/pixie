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

package main

import (
	"fmt"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/vizier/seq_tests/client/pkg/httpclient"
)

func init() {
	pflag.String("http_host", "", "The host of the http server")
	pflag.String("http_path", "", "The request path to request from the http server")
	pflag.Int("http_port", 0, "Port of the http server")

	pflag.Int("num_connections", 0, "Number of simulataneous connections for the seq load generator")
	pflag.Int("target_rps", 0, "Target requests per second across all connections")
	pflag.Int("req_size", 1024, "Size of the request in bytes")
	pflag.Int("resp_size", 1024, "Size of the response in bytes")
	pflag.Int("num_messages", 1000, "Num messages per conn per loop")
}

func main() {
	viper.AutomaticEnv()
	viper.BindPFlags(pflag.CommandLine)

	host := viper.GetString("http_host")
	path := viper.GetString("http_path")
	port := viper.GetInt("http_port")
	addr := fmt.Sprintf("http://%s:%d%s", host, port, path)

	numConns := viper.GetInt("num_connections")
	targetRPS := viper.GetInt("target_rps")
	numMessagesPerConn := viper.GetInt("num_messages")
	reqSize := viper.GetInt("req_size")
	respSize := viper.GetInt("resp_size")

	numMessages := numMessagesPerConn * numConns

	seqNum := 0
	for {
		log.Infof("Started loadtest with %d conns, %d messages, %d req_size, %d resp_size, %d target_rps", numConns, numMessages, reqSize, respSize, targetRPS)
		c := httpclient.New(addr, seqNum, numMessages, numConns, reqSize, respSize, targetRPS)
		err := c.Run()
		if err != nil {
			log.WithError(err).Error("failed to run seq client")
		}
		err = c.PrintStats()
		if err != nil {
			log.WithError(err).Error("failed to print stats")
		}
		seqNum += numMessages + 1
	}
}
