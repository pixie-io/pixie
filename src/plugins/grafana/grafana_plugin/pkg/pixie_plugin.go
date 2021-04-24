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
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"github.com/grafana/grafana-plugin-sdk-go/backend"
	"github.com/grafana/grafana-plugin-sdk-go/backend/datasource"
	"github.com/grafana/grafana-plugin-sdk-go/backend/instancemgmt"
	"github.com/grafana/grafana-plugin-sdk-go/backend/log"

	"px.dev/pixie/src/api/go/pxapi"
)

// GrafanaMacro is a type which defines a macro.
type GrafanaMacro string

const (
	// Define keys to retrieve configs passed from UI.
	apiKeyStr       = "apiKey"
	clusterIDKeyStr = "clusterId"
	// timeFromMacro is the start of the time range of a query.
	timeFromMacro GrafanaMacro = "$__timeFrom()"
	// timeToMacro is the end of the time range of a query.
	timeToMacro GrafanaMacro = "$__timeTo()"
	// intervalMacro is the suggested duration between time points.
	intervalMacro GrafanaMacro = "$__interval"
)

// replaceTimeMacroInQueryText takes the query text (PxL script to execute)
// and replaces the time macros with the relevant time objects.
func replaceTimeMacroInQueryText(queryText string, grafanaMacro GrafanaMacro,
	timeReplacement time.Time) string {
	tStr := fmt.Sprintf("%d", timeReplacement.UnixNano())
	return strings.ReplaceAll(queryText, string(grafanaMacro), tStr)
}

// replaceIntervalMacroInQueryText takes the query text and replaces
// interval macro with interval duration specified in Grafana UI.
func replaceIntervalMacroInQueryText(queryText string, grafanaMacro GrafanaMacro,
	intervalDuration time.Duration) string {
	intervalStr := fmt.Sprintf("%d", intervalDuration.Nanoseconds())
	return strings.ReplaceAll(queryText, string(grafanaMacro), intervalStr)
}

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
func (td *PixieDatasource) QueryData(ctx context.Context, req *backend.QueryDataRequest) (
	*backend.QueryDataResponse, error) {
	response := backend.NewQueryDataResponse()
	// Loop over queries and execute them individually. Save the response
	// in a hashmap with RefID as identifier.
	for _, q := range req.Queries {
		res, err := td.query(ctx, q, req.PluginContext.DataSourceInstanceSettings.DecryptedSecureJSONData)
		if err != nil {
			return response, err
		}
		response.Responses[q.RefID] = *res
	}

	return response, nil
}

type queryModel struct {
	QueryText string `json:"queryText"`
}

func (td *PixieDatasource) query(ctx context.Context, query backend.DataQuery,
	config map[string]string) (*backend.DataResponse, error) {
	// Unmarshal the json into our queryModel.
	var qm queryModel
	err := json.Unmarshal(query.JSON, &qm)
	if err != nil {
		return nil, fmt.Errorf("Json Unmarshal Error %v", err)
	}

	// API Token.
	apiTokenStr := config[apiKeyStr]

	// Create a Pixie client.
	client, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(apiTokenStr))
	if err != nil {
		log.DefaultLogger.Warn("Unable to create Pixie Client.")
		return nil, err
	}

	// Create a connection to the cluster.
	clusterIDStr := config[clusterIDKeyStr]

	vz, err := client.NewVizierClient(ctx, clusterIDStr)
	if err != nil {
		log.DefaultLogger.Warn("Unable to create Vizier Client.")
		return nil, err
	}

	response := &backend.DataResponse{}

	// Create TableMuxer to accept results table.
	tm := &PixieToGrafanaTableMux{}

	// Update macros in query text.
	qm.QueryText = replaceTimeMacroInQueryText(qm.QueryText, timeFromMacro,
		query.TimeRange.From)
	qm.QueryText = replaceTimeMacroInQueryText(qm.QueryText, timeToMacro,
		query.TimeRange.To)
	qm.QueryText = replaceIntervalMacroInQueryText(qm.QueryText, intervalMacro,
		query.Interval)

	// Execute the PxL script.
	resultSet, err := vz.ExecuteScript(ctx, qm.QueryText, tm)
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
