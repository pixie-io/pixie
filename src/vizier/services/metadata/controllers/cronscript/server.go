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

package cronscript

import (
	"context"
	"sync"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

// Store is a datastore which can store, update, and retrieve information about cron scripts.
type Store interface {
	GetCronScripts() ([]*cvmsgspb.CronScript, error)
	UpsertCronScript(script *cvmsgspb.CronScript) error
	DeleteCronScript(id uuid.UUID) error
	SetCronScripts(scripts []*cvmsgspb.CronScript) error
	RecordCronScriptResult(*storepb.CronScriptResult) error
	GetAllCronScriptResults() ([]*storepb.CronScriptResult, error)
}

// Server is an implementation of the cronscriptstore service.
type Server struct {
	ds Store

	done chan struct{}
	once sync.Once
}

// New creates a new server.
func New(ds Store) *Server {
	return &Server{
		ds:   ds,
		done: make(chan struct{}),
	}
}

// Stop performs any necessary cleanup before shutdown.
func (s *Server) Stop() {
	s.once.Do(func() {
		close(s.done)
	})
}

// GetScripts fetches all scripts in the cron script store.
func (s *Server) GetScripts(ctx context.Context, req *metadatapb.GetScriptsRequest) (*metadatapb.GetScriptsResponse, error) {
	scripts, err := s.ds.GetCronScripts()
	if err != nil {
		return nil, err
	}

	scMap := make(map[string]*cvmsgspb.CronScript)
	for _, s := range scripts {
		id := utils.UUIDFromProtoOrNil(s.ID)
		scMap[id.String()] = s
	}

	return &metadatapb.GetScriptsResponse{
		Scripts: scMap,
	}, nil
}

// AddOrUpdateScript updates or adds a cron script to the store, based on ID.
func (s *Server) AddOrUpdateScript(ctx context.Context, req *metadatapb.AddOrUpdateScriptRequest) (*metadatapb.AddOrUpdateScriptResponse, error) {
	err := s.ds.UpsertCronScript(req.Script)
	if err != nil {
		return nil, err
	}
	return &metadatapb.AddOrUpdateScriptResponse{}, nil
}

// DeleteScript deletes a cron script from the store by ID.
func (s *Server) DeleteScript(ctx context.Context, req *metadatapb.DeleteScriptRequest) (*metadatapb.DeleteScriptResponse, error) {
	err := s.ds.DeleteCronScript(utils.UUIDFromProtoOrNil(req.ScriptID))
	if err != nil {
		return nil, err
	}
	return &metadatapb.DeleteScriptResponse{}, nil
}

// SetScripts sets the list of all cron scripts to match the given set of scripts.
func (s *Server) SetScripts(ctx context.Context, req *metadatapb.SetScriptsRequest) (*metadatapb.SetScriptsResponse, error) {
	scripts := make([]*cvmsgspb.CronScript, 0)
	for _, v := range req.Scripts {
		scripts = append(scripts, v)
	}

	return &metadatapb.SetScriptsResponse{}, s.ds.SetCronScripts(scripts)
}

// RecordExecutionResult records the stats of a successful CronScript execution or the error message of an unsuccessful execution.
func (s *Server) RecordExecutionResult(ctx context.Context, req *metadatapb.RecordExecutionResultRequest) (*metadatapb.RecordExecutionResultResponse, error) {
	result := &storepb.CronScriptResult{
		ScriptID:  req.GetScriptID(),
		Timestamp: req.Timestamp,
		Error:     req.GetError(),
	}
	if execStats := req.GetExecutionStats(); execStats != nil {
		result.ExecutionTimeNs = execStats.ExecutionTimeNs
		result.CompilationTimeNs = execStats.CompilationTimeNs
		result.BytesProcessed = execStats.BytesProcessed
		result.RecordsProcessed = execStats.RecordsProcessed
	}

	err := s.ds.RecordCronScriptResult(result)
	if err != nil {
		return nil, err
	}
	return &metadatapb.RecordExecutionResultResponse{}, nil
}

// GetAllExecutionResults returns all of the execution results for cronscripts stored by this service.
func (s *Server) GetAllExecutionResults(ctx context.Context, req *metadatapb.GetAllExecutionResultsRequest) (*metadatapb.GetAllExecutionResultsResponse, error) {
	results, err := s.ds.GetAllCronScriptResults()
	if err != nil {
		return nil, err
	}
	resp := &metadatapb.GetAllExecutionResultsResponse{
		Results: make([]*metadatapb.GetAllExecutionResultsResponse_ExecutionResult, len(results)),
	}

	for i, res := range results {
		newResult := &metadatapb.GetAllExecutionResultsResponse_ExecutionResult{
			ScriptID:  res.ScriptID,
			Timestamp: res.Timestamp,
		}
		if res.Error != nil {
			newResult.Result = &metadatapb.GetAllExecutionResultsResponse_ExecutionResult_Error{
				Error: res.Error,
			}
		} else {
			newResult.Result = &metadatapb.GetAllExecutionResultsResponse_ExecutionResult_ExecutionStats{
				ExecutionStats: &metadatapb.ExecutionStats{
					ExecutionTimeNs:   res.ExecutionTimeNs,
					CompilationTimeNs: res.CompilationTimeNs,
					BytesProcessed:    res.BytesProcessed,
					RecordsProcessed:  res.RecordsProcessed,
				},
			}
		}

		resp.Results[i] = newResult
	}
	return resp, nil
}
