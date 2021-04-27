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
	"context"
	"fmt"
	"os"

	"px.dev/pixie/src/api/go/pxapi"
)

func main() {
	apiKey, ok := os.LookupEnv("PX_API_KEY")
	if !ok {
		panic("please set PX_API_KEY")
	}

	ctx := context.Background()
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(apiKey))
	if err != nil {
		panic(err)
	}

	key, err := client.CreateDeployKey(ctx, "test key")
	if err != nil {
		panic(err)
	}
	fmt.Printf("Created deploy key: %v\n", key)
}
