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
	"log"
	"os"
	"strconv"

	"px.dev/pixie/src/e2e_test/protocol_loadtest/grpc"
	"px.dev/pixie/src/e2e_test/protocol_loadtest/http"
)

func main() {
	httpNumBytesHeaders, err := strconv.Atoi(os.Getenv("HTTP_NUM_BYTES_HEADERS"))
	if err != nil {
		log.Fatalln("Must specify valid integer HTTP_NUM_BYTES_HEADERS in environment")
	}
	httpNumBytesBody, err := strconv.Atoi(os.Getenv("HTTP_NUM_BYTES_BODY"))
	if err != nil {
		log.Fatalln("Must specify valid integer HTTP_NUM_BYTES_BODY in environment")
	}
	go http.RunHTTPServers(os.Getenv("HTTP_PORT"), os.Getenv("HTTP_SSL_PORT"), httpNumBytesHeaders,
		httpNumBytesBody)
	grpc.RunGRPCServers(os.Getenv("GRPC_PORT"))
}
