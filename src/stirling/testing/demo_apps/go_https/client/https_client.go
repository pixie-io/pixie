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
	"crypto/tls"
	"fmt"
	"io"
	"log"
	"net/http"
	"runtime"
	"time"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"golang.org/x/net/http2"
)

const (
	address = "https://127.0.0.1:50101"
)

// Use this HTTPS client with https_server.go, which listens on HTTPS port 50101.

func main() {
	pflag.Int("max_procs", 1, "The maximum number of OS threads created by the golang runtime.")
	pflag.Int("iters", 1000, "Number of iterations.")
	pflag.Int("sub_iters", 1000, "Number of sub-iterations with same TLS config.")
	pflag.Bool("http2", true, "Use HTTP/2, instead of HTTP/1.1.")
	pflag.Parse()

	viper.BindPFlags(pflag.CommandLine)
	useHTTP2 := viper.GetBool("http2")
	log.Printf("Starting HTTPS client. HTTP/2 enabled: %t", useHTTP2)

	runtime.GOMAXPROCS(viper.GetInt("max_procs"))

	for i := 0; i < viper.GetInt("iters"); i++ {
		client := &http.Client{}
		tlsConfig := &tls.Config{InsecureSkipVerify: true}

		if useHTTP2 {
			client.Transport = &http2.Transport{TLSClientConfig: tlsConfig}
		} else {
			client.Transport = &http.Transport{TLSClientConfig: tlsConfig}
		}

		for j := 0; j < viper.GetInt("sub_iters"); j++ {
			resp, err := client.Get(address)
			if err != nil {
				fmt.Println(err)
			}

			body, err := io.ReadAll(resp.Body)
			if err != nil {
				log.Fatalln(err)
			} else {
				log.Println(string(body))
			}
			time.Sleep(time.Second)
		}
	}
}
