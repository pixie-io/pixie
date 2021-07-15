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
	"log"
	"time"

	"github.com/nats-io/nats.go"
)

func main() {
	address := flag.String("address", "localhost:4222", "Server end point.")

	flag.Parse()

	nc, _ := nats.Connect(*address)

	// Simple Sync Subscriber
	sub, err := nc.SubscribeSync("foo")
	if err != nil {
		log.Fatal(err)
	}

	err = nc.Publish("foo", []byte("Hello World"))
	if err != nil {
		log.Fatal(err)
	}

	m, err := sub.NextMsg(100 * time.Second)
	if err != nil {
		log.Fatal(err)
	}
	fmt.Print(m)

	err = sub.Unsubscribe()
	if err != nil {
		log.Fatal(err)
	}

	err = sub.Drain()
	if err != nil {
		log.Fatal(err)
	}

	nc.Close()
}
