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
 */

package main

import (
	"context"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"strings"
	"time"

	"github.com/slack-go/slack"
	"go.withpixie.dev/pixie/src/api/go/pxapi"
	"go.withpixie.dev/pixie/src/api/go/pxapi/errdefs"
	"go.withpixie.dev/pixie/src/api/go/pxapi/types"
)

func main() {

	// Slack channel for Slackbot to post in.
	// Slack App must be a member of this channel.
	slackChannel := "#pixie-alerts"

	// This PxL script ouputs a table of the HTTP total requests count and
	// HTTP error (>4xxx) count for each service in the `px-sock-shop` namespace.
	// To deploy the px-sock-shop demo, see:
	// https://docs.pixielabs.ai/tutorials/slackbot-alert for how to
	b, err := ioutil.ReadFile("http_errors.pxl")
	if err != nil {
		panic(err)
	}
	pxlScript := string(b)

	// The slackbot requires the following configs, which are specified
	// using environment variables. For directions on how to find these
	// config values, see: https://docs.pixielabs.ai/tutorials/slackbot-alert
	pixieAPIKey, ok := os.LookupEnv("PIXIE_API_KEY")
	if !ok {
		panic("Please set PIXIE_API_KEY environment variable.")
	}

	pixieClusterID, ok := os.LookupEnv("PIXIE_CLUSTER_ID")
	if !ok {
		panic("Please set PIXIE_CLUSTER_ID environment variable.")
	}

	slackToken, ok := os.LookupEnv("SLACK_BOT_TOKEN")
	if !ok {
		panic("Please set SLACK_BOT_TOKEN environment variable.")
	}

	ctx := context.Background()
	pixieClient, err := pxapi.NewClient(ctx, pxapi.WithAPIKey(pixieAPIKey))
	if err != nil {
		panic(err)
	}
	vz, err := pixieClient.NewVizierClient(ctx, pixieClusterID)
	if err != nil {
		panic(err)
	}

	slackClient := slack.New(slackToken)

	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		tm := &tableMux{tables: make(map[string]*tableCollector)}
		log.Println("Executing PxL script.")
		resultSet, err := vz.ExecuteScript(ctx, pxlScript, tm)
		if err != nil {
			panic(err)
		}

		log.Println("Stream PxL script results.")
		if err := resultSet.Stream(); err != nil {
			log.Printf("Got error: %+v, while streaming.\n", err)
		}

		// Get slack message constructed from table data.
		table := tm.GetTable("http_table").GetTableDataSync()
		log.Println("Sending slack message.")
		_, _, err = slackClient.PostMessage(slackChannel, slack.MsgOptionText(table, false), slack.MsgOptionAsUser(true))
		if err != nil {
			log.Println("Error sending to slack: " + err.Error())
		}

		// wait for next tick
		<-ticker.C
	}
}

// Implement the TableRecordHandler interface to processes the PxL script output table record-wise.
type tableCollector struct {
	tableDataBuilder strings.Builder
	// Channel used to block until all of the table data to be collected.
	done chan struct{}
}

func (t *tableCollector) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	t.tableDataBuilder.WriteString("*Recent 4xx+ Spikes in last 5 minutes:*\n")
	return nil
}

func (t *tableCollector) HandleRecord(ctx context.Context, r *types.Record) error {
	fmt.Fprintf(&t.tableDataBuilder, "`%s` \t ---> %s  (>4xx) errors out of %s requests.\n",
		r.GetDatum("service"), r.GetDatum("error_count"), r.GetDatum("total_requests"))
	return nil
}

func (t *tableCollector) HandleDone(ctx context.Context) error {
	close(t.done)
	return nil
}

func (t *tableCollector) GetTableDataSync() string {
	// Wait until the `done` channel is closed, indicating table data has finished collecting.
	<- t.done
	return t.tableDataBuilder.String()
}

// Implement the TableMuxer to route pxl script output tables to the correct handler.
type tableMux struct {
	tables map[string]*tableCollector
}

func (s *tableMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	s.tables[metadata.Name] = &tableCollector{done: make(chan struct{})}
	return s.tables[metadata.Name], nil
}

func (s *tableMux) GetTable(tableName string) *tableCollector {
	return s.tables[tableName]
}
