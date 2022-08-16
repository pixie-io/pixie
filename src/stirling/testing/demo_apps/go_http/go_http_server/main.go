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
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"strconv"
)

type helloReply struct {
	Greeter string `json:"greeter"`
}

func handleSayHello(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "json")
	name := "world"
	nameArgs, ok := r.URL.Query()["name"]
	if ok {
		name = nameArgs[0]
	}
	reply := helloReply{Greeter: "Hello " + name + "!"}
	err := json.NewEncoder(w).Encode(reply)
	if err != nil {
		log.Fatal(err)
	}
}

func handlePost(w http.ResponseWriter, r *http.Request) {
	// Do nothing.
}

func main() {
	port := flag.Int("port", 0, "The port number to serve.")
	flag.Parse()

	listener, err := net.Listen("tcp", ":"+strconv.Itoa(*port))
	if err != nil {
		log.Fatal(err)
	}

	fmt.Print(listener.Addr().(*net.TCPAddr).Port)

	http.HandleFunc("/sayhello", handleSayHello)
	http.HandleFunc("/post", handlePost)
	err = http.Serve(listener, nil)
	if err != nil {
		log.Fatal(err)
	}
}
