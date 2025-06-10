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
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"

	"golang.org/x/net/http2"
)

const (
	httpPort  = 50100
	httpsPort = 50101
)

// Import the http2 package to ensure golang.org/x/net exists within the binary's
// buildinfo.
var s http2.Server //nolint:unused

func basicHandler(w http.ResponseWriter, r *http.Request) {
	w.Header().Add("Content-Type", "application/json")
	_, err := io.WriteString(w, `{"status":"ok"}`)
	if err != nil {
		log.Fatal(err)
	}
}

func listenAndServeTLS(port int, certFile, keyFile string) {
	log.Printf("Starting HTTPS service on Port %d", port)
	err := http.ListenAndServeTLS(fmt.Sprintf(":%d", port), certFile, keyFile, nil)
	if err != nil {
		log.Fatal(err)
	}
}

func listenAndServe(port int) {
	log.Printf("Starting HTTP service on Port %d", port)
	err := http.ListenAndServe(fmt.Sprintf(":%d", port), nil)
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	certPath := flag.String("cert", "", "Path to the .crt file.")
	keyPath := flag.String("key", "", "Path to the .key file.")
	flag.Parse()

	http.HandleFunc("/", basicHandler)

	go listenAndServeTLS(httpsPort, *certPath, *keyPath)
	listenAndServe(httpPort)
}
