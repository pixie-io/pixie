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
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"os"

	"github.com/grafana/grafana-plugin-sdk-go/backend"
	"github.com/grafana/grafana-plugin-sdk-go/backend/datasource"
	"github.com/grafana/grafana-plugin-sdk-go/backend/instancemgmt"
	"github.com/grafana/grafana-plugin-sdk-go/backend/log"

	"px.dev/pixie/src/api/go/pxapi"
)

// Define hardcoded PxL script.
// TODO(vjain, PC-830): Get Pxl script from frontend instead of
// hardcoding.
var (
	pxl = `
import px
df = px.DataFrame(table='process_stats', start_time='-1m')
val = px.uint128("00000001-003d-3020-0000-000029ff124f")
df = df[df.upid == val]
dfOne = df['time_','rss_bytes', 'vsize_bytes'].head(10)
dfTwo = df['time_','major_faults'].head(10)
px.display(dfOne)
px.display(dfTwo)
`
)

// newDatasource returns datasource.ServeOpts.
func newDatasource() datasource.ServeOpts {
	// Creates a instance manager for your plugin. The function passed
	// into `NewInstanceManger` is called when the instance is created
	// for the first time or when a datasource configuration changed.
	im := datasource.NewInstanceManager(newDataSourceInstance)
	ds := &PixieDatasource{
		im: im,
	}

	return datasource.ServeOpts{
		QueryDataHandler:   ds,
		CheckHealthHandler: ds,
	}
}

// PixieDatasource is an example datasource used to scaffold
// new datasource plugins with an backend.
type PixieDatasource struct {
	// The instance manager can help with lifecycle management
	// of datasource instances in plugins. It's not a requirements
	// but a best practice that we recommend that you follow.
	im instancemgmt.InstanceManager
}

// QueryData handles multiple queries and returns multiple responses.
// req contains the queries []DataQuery (where each query contains RefID
// as a unique identifier). The QueryDataResponse contains a map of RefID
// to the response for each query, and each response contains Frames ([]*Frame).
func (td *PixieDatasource) QueryData(ctx context.Context, req *backend.QueryDataRequest) (*backend.QueryDataResponse, error) {
	response := backend.NewQueryDataResponse()
	// Loop over queries and execute them individually. Save the response
	// in a hashmap with RefID as identifier.
	for _, q := range req.Queries {
		res, err := td.query(ctx, q)
		if err != nil {
			return response, err
		}
		response.Responses[q.RefID] = *res
	}

	return response, nil
}

type queryModel struct {
	Format string `json:"format"`
}

func (td *PixieDatasource) query(ctx context.Context, query backend.DataQuery) (*backend.DataResponse,
	error) {
	// Unmarshal the json into our queryModel.
	var qm queryModel
	err := json.Unmarshal(query.JSON, &qm)
	if err != nil {
		return nil, fmt.Errorf("Json Unmarshal Error %v", err)
	}

	if qm.Format == "" {
		log.DefaultLogger.Warn("format is empty. defaulting to time series")
	}

	// API Token.
	// TODO(vjain, PC-830): get the API key and the cluster ID
	// from the QueryDataRequest.
	apiTokenStr, ok := os.LookupEnv("PX_API_KEY")
	if !ok {
		return nil, errors.New("failed to lookup Pixie API Key")
	}

	// Create a Pixie client.
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(apiTokenStr))
	if err != nil {
		log.DefaultLogger.Warn("Unable to create Pixie Client.")
		return nil, err
	}

	// Create a connection to the cluster.
	// TODO(vjain, PC-830): get the API key and the cluster ID
	// from the QueryDataRequest.
	clusterIDStr, ok := os.LookupEnv("PX_CLUSTER_ID")
	if !ok {
		return nil, errors.New("failed to lookup Cluster Id")
	}

	vz, err := client.NewVizierClient(ctx, clusterIDStr)
	if err != nil {
		log.DefaultLogger.Warn("Unable to create Vizier Client.")
		return nil, err
	}

	response := &backend.DataResponse{}

	// Create TableMuxer to accept results table.
	tm := &PixieToGrafanaTableMux{}

	// Execute the PxL script.
	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil && err != io.EOF {
		log.DefaultLogger.Warn("Can't execute script.")
		return nil, err
	}

	// Receive the PxL script results.
	defer resultSet.Close()
	if err := resultSet.Stream(); err != nil {
		streamStrErr := fmt.Errorf("got error : %+v, while streaming", err)
		response.Error = streamStrErr
		log.DefaultLogger.Error(streamStrErr.Error())
	}

	// Add the frames to the response.
	for _, tableFrame := range tm.pxTablePrinterLst {
		response.Frames = append(response.Frames,
			tableFrame.frame)
	}
	return response, nil
}

// CheckHealth handles health checks sent from Grafana to the plugin.
// The main use case for these health checks is the test button on the
// datasource configuration page which allows users to verify that
// a datasource is working as expected.
func (td *PixieDatasource) CheckHealth(ctx context.Context, req *backend.CheckHealthRequest) (*backend.CheckHealthResult, error) {
	status := backend.HealthStatusOk
	message := "Data source is working"

	return &backend.CheckHealthResult{
		Status:  status,
		Message: message,
	}, nil
}

type instanceSettings struct {
	httpClient *http.Client
}

func newDataSourceInstance(setting backend.DataSourceInstanceSettings) (instancemgmt.Instance, error) {
	return &instanceSettings{
		httpClient: &http.Client{},
	}, nil
}

func (s *instanceSettings) Dispose() {
	// Called before creatinga a new instance to allow plugin authors
	// to cleanup.
}
