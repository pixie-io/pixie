// Copyright 2018- The Pixie Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package pxl

import (
	"context"
	"fmt"

	log "github.com/sirupsen/logrus"
	"px.dev/pixie/src/api/go/pxapi"
	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
)

// recordCounter counts the number of records received
type recordCounter struct {
	count int
}

func (r *recordCounter) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	return nil
}

func (r *recordCounter) HandleRecord(ctx context.Context, record *types.Record) error {
	r.count++
	return nil
}

func (r *recordCounter) HandleDone(ctx context.Context) error {
	return nil
}

type recordCounterMux struct {
	counter *recordCounter
}

func (m *recordCounterMux) AcceptTable(ctx context.Context, metadata types.TableMetadata) (pxapi.TableRecordHandler, error) {
	return m.counter, nil
}

// ExecuteScript executes a PxL script and returns the number of records returned
func ExecuteScript(ctx context.Context, client *pxapi.Client, clusterID string, pxl string) (int, error) {
	vz, err := client.NewVizierClient(ctx, clusterID)
	if err != nil {
		return 0, fmt.Errorf("failed to create vizier client: %w", err)
	}

	counter := &recordCounter{}
	tm := &recordCounterMux{counter: counter}

	resultSet, err := vz.ExecuteScript(ctx, pxl, tm)
	if err != nil {
		return 0, fmt.Errorf("failed to execute script: %w", err)
	}
	defer resultSet.Close()

	if err := resultSet.Stream(); err != nil {
		if errdefs.IsCompilationError(err) {
			return 0, fmt.Errorf("PxL compilation error: %w", err)
		}
		return 0, fmt.Errorf("error streaming results: %w", err)
	}

	log.Debugf("Script execution time: %v, bytes received: %v", resultSet.Stats().ExecutionTime, resultSet.Stats().TotalBytes)
	return counter.count, nil
}
