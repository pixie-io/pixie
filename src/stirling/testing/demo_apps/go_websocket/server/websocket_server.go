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
	"net"
	"net/http"

	"golang.org/x/net/websocket"
)

func echo(ws *websocket.Conn) {
	if _, err := io.Copy(ws, ws); err != nil {
		log.Printf("echo: %v", err)
	}
}

func main() {
	wsPort := flag.Int("ws_port", 50200, "Port of the plaintext WebSocket listener.")
	wssPort := flag.Int("wss_port", 50201, "Port of the TLS WebSocket listener.")
	certPath := flag.String("cert", "", "Path to the .crt file.")
	keyPath := flag.String("key", "", "Path to the .key file.")
	flag.Parse()

	http.Handle("/echo", websocket.Handler(echo))

	wsListener, err := net.Listen("tcp", fmt.Sprintf(":%d", *wsPort))
	if err != nil {
		log.Fatal(err)
	}
	wssListener, err := net.Listen("tcp", fmt.Sprintf(":%d", *wssPort))
	if err != nil {
		log.Fatal(err)
	}
	go func() {
		log.Fatal(http.ServeTLS(wssListener, nil, *certPath, *keyPath))
	}()
	log.Printf("Starting WebSocket service on ports %d and %d", *wsPort, *wssPort)
	log.Fatal(http.Serve(wsListener, nil))
}
