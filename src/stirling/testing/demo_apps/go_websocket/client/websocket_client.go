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
	"bytes"
	"crypto/tls"
	"flag"
	"fmt"
	"log"

	"golang.org/x/net/websocket"
)

const subprotocol = "echo.pixie"

func exchange(url string, iters int) {
	config, err := websocket.NewConfig(url, "http://localhost/")
	if err != nil {
		log.Fatal(err)
	}
	config.Protocol = []string{subprotocol}
	config.TlsConfig = &tls.Config{InsecureSkipVerify: true}
	ws, err := websocket.DialConfig(config)
	if err != nil {
		log.Fatal(err)
	}
	defer ws.Close()

	for i := 0; i < iters; i++ {
		msg := fmt.Sprintf("hello-%d", i)
		if err := websocket.Message.Send(ws, msg); err != nil {
			log.Fatal(err)
		}
		var reply string
		if err := websocket.Message.Receive(ws, &reply); err != nil {
			log.Fatal(err)
		}
		if reply != msg {
			log.Fatalf("got %q, want %q", reply, msg)
		}
	}

	binary := []byte{0x01, 0x02, 'h', 'i'}
	if err := websocket.Message.Send(ws, binary); err != nil {
		log.Fatal(err)
	}
	var reply []byte
	if err := websocket.Message.Receive(ws, &reply); err != nil {
		log.Fatal(err)
	}
	if !bytes.Equal(reply, binary) {
		log.Fatalf("got %q, want %q", reply, binary)
	}
}

func main() {
	address := flag.String("address", "127.0.0.1", "Server address.")
	wsPort := flag.Int("ws_port", 50200, "Port of the plaintext WebSocket listener.")
	wssPort := flag.Int("wss_port", 50201, "Port of the TLS WebSocket listener.")
	iters := flag.Int("iters", 3, "Number of text messages per connection.")
	flag.Parse()

	exchange(fmt.Sprintf("ws://%s:%d/echo", *address, *wsPort), *iters)
	exchange(fmt.Sprintf("wss://%s:%d/echo", *address, *wssPort), *iters)
	log.Print("WebSocket client done")
}
