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
	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
)

var (
	pxl = `
import px
df = px.DataFrame('http_events', start_time='-5m')[['resp_status','req_path']]
px.display(df.stream(), 'http_table')
`
)

type tablePrinter struct{}

func (t *tablePrinter) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	return nil
}

func (t *tablePrinter) HandleRecord(ctx context.Context, r *types.Record) error {
	for _, d := range r.Data {
		fmt.Printf("%s ", d.String())
	}
	fmt.Printf("\n")
	return nil
}

func (t *tablePrinter) HandleDone(ctx context.Context) error {
	return nil
}

type tableMux struct {
}

func (s *tableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	return &tablePrinter{}, nil
}

func main() {
	apiKey, ok := os.LookupEnv("PX_API_KEY")
	if !ok {
		panic("please set PX_API_KEY")
	}
	clusterID, ok := os.LookupEnv("PX_CLUSTER_ID")
	if !ok {
		panic("please set PX_CLUSTER_ID")
	}

	ctx := context.Background()
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(apiKey))
	if err != nil {
		panic(err)
	}

	fmt.Printf("Running on Cluster: %s\n", clusterID)

	tm := &tableMux{}

	fmt.Println("Running script")
	vz, err := client.NewVizierClient(ctx, clusterID)
	if err != nil {
		panic(err)
	}

	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil {
		panic(err)
	}

	defer resultSet.Close()
	if err := resultSet.Stream(); err != nil {
		if errdefs.IsCompilationError(err) {
			fmt.Printf("Got compiler error: \n %s\n", err.Error())
		} else {
			fmt.Printf("Got error : %+v, while streaming\n", err)
		}
	}

	stats := resultSet.Stats()
	fmt.Printf("Execution Time: %v\n", stats.ExecutionTime)
	fmt.Printf("Bytes received: %v\n", stats.TotalBytes)
}
