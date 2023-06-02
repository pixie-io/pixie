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

package scriptrunner

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"google.golang.org/grpc"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

type TestSource struct {
	stopDelegate  func()
	startDelegate func(baseCtx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error)
}

func (m *TestSource) Stop() {
	m.stopDelegate()
}

func (m *TestSource) Start(baseCtx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
	return m.startDelegate(baseCtx, updatesCh)
}

func requireNoReceive[T any](t *testing.T, messages chan T, timeout time.Duration) {
	t.Helper()
	select {
	case <-messages:
		t.Fatal("should not receive any messages")
	case <-time.After(timeout):
		// it works
	}
}

func requireReceiveWithin[T any](t *testing.T, messages chan T, timeout time.Duration) T {
	t.Helper()
	var message T
	select {
	case message = <-messages:
	case <-time.After(timeout):
		t.Fatalf("did not receive a message within %s", timeout.String())
	}
	return message
}

func fakeSource(scripts map[string]*cvmsgspb.CronScript, stop func(), err error) Source {
	return &TestSource{
		startDelegate: func(_ context.Context, _ chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
			return scripts, err
		},
		stopDelegate: func() {
			stop()
		},
	}
}

func dummySource() Source {
	return &TestSource{
		startDelegate: func(_ context.Context, _ chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
			return nil, nil
		},
		stopDelegate: func() {},
	}
}

func errorSource(err error) Source {
	return &TestSource{
		startDelegate: func(_ context.Context, _ chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
			return nil, err
		},
		stopDelegate: func() {},
	}
}

type fakeCronStore struct {
	scriptsMu               sync.Mutex
	scripts                 map[uuid.UUID]*cvmsgspb.CronScript
	receivedResultRequestCh chan<- *metadatapb.RecordExecutionResultRequest
}

// GetScripts fetches all scripts in the cron script store.
func (s *fakeCronStore) GetScripts(ctx context.Context, req *metadatapb.GetScriptsRequest, opts ...grpc.CallOption) (*metadatapb.GetScriptsResponse, error) {
	s.scriptsMu.Lock()
	defer s.scriptsMu.Unlock()
	if s.scripts == nil {
		return &metadatapb.GetScriptsResponse{}, nil
	}
	m := make(map[string]*cvmsgspb.CronScript)
	for k, v := range s.scripts {
		m[k.String()] = v
	}

	return &metadatapb.GetScriptsResponse{
		Scripts: m,
	}, nil
}

func (s *fakeCronStore) Scripts() map[uuid.UUID]*cvmsgspb.CronScript {
	s.scriptsMu.Lock()
	defer s.scriptsMu.Unlock()
	result := make(map[uuid.UUID]*cvmsgspb.CronScript, len(s.scripts))
	for id, script := range s.scripts {
		result[id] = script
	}
	return result
}

// AddOrUpdateScript updates or adds a cron script to the store, based on ID.
func (s *fakeCronStore) AddOrUpdateScript(ctx context.Context, req *metadatapb.AddOrUpdateScriptRequest, opts ...grpc.CallOption) (*metadatapb.AddOrUpdateScriptResponse, error) {
	s.scriptsMu.Lock()
	defer s.scriptsMu.Unlock()
	if s.scripts == nil {
		s.scripts = map[uuid.UUID]*cvmsgspb.CronScript{}
	}
	s.scripts[utils.UUIDFromProtoOrNil(req.Script.ID)] = req.Script

	return &metadatapb.AddOrUpdateScriptResponse{}, nil
}

// DeleteScript deletes a cron script from the store by ID.
func (s *fakeCronStore) DeleteScript(ctx context.Context, req *metadatapb.DeleteScriptRequest, opts ...grpc.CallOption) (*metadatapb.DeleteScriptResponse, error) {
	s.scriptsMu.Lock()
	defer s.scriptsMu.Unlock()
	_, ok := s.scripts[utils.UUIDFromProtoOrNil(req.ScriptID)]
	if ok {
		delete(s.scripts, utils.UUIDFromProtoOrNil(req.ScriptID))
	}

	return &metadatapb.DeleteScriptResponse{}, nil
}

// SetScripts sets the list of all cron scripts to match the given set of scripts.
func (s *fakeCronStore) SetScripts(ctx context.Context, req *metadatapb.SetScriptsRequest, opts ...grpc.CallOption) (*metadatapb.SetScriptsResponse, error) {
	s.scriptsMu.Lock()
	defer s.scriptsMu.Unlock()
	m := make(map[uuid.UUID]*cvmsgspb.CronScript)
	for k, v := range req.Scripts {
		m[uuid.FromStringOrNil(k)] = v
	}
	s.scripts = m

	return &metadatapb.SetScriptsResponse{}, nil
}

// RecordExecutionResult stores the result of execution, whether that's an error or the stats about the execution.
func (s *fakeCronStore) RecordExecutionResult(ctx context.Context, req *metadatapb.RecordExecutionResultRequest, opts ...grpc.CallOption) (*metadatapb.RecordExecutionResultResponse, error) {
	s.receivedResultRequestCh <- req
	return &metadatapb.RecordExecutionResultResponse{}, nil
}

// RecordExecutionResult stores the result of execution, whether that's an error or the stats about the execution.
func (s *fakeCronStore) GetAllExecutionResults(ctx context.Context, req *metadatapb.GetAllExecutionResultsRequest, opts ...grpc.CallOption) (*metadatapb.GetAllExecutionResultsResponse, error) {
	return &metadatapb.GetAllExecutionResultsResponse{}, nil
}

type fakeExecuteScriptClient struct {
	// The error to send if not nil. The informer does not send responses if this is not nil.
	err       error
	responses []*vizierpb.ExecuteScriptResponse
	responseI int
	grpc.ClientStream
}

func (es *fakeExecuteScriptClient) Recv() (*vizierpb.ExecuteScriptResponse, error) {
	if es.err != nil {
		return nil, es.err
	}

	resp := es.responses[es.responseI]
	es.responseI++
	return resp, nil
}

type fakeVizierServiceClient struct {
	responses []*vizierpb.ExecuteScriptResponse
	err       error
}

func (vs *fakeVizierServiceClient) ExecuteScript(ctx context.Context, in *vizierpb.ExecuteScriptRequest, opts ...grpc.CallOption) (vizierpb.VizierService_ExecuteScriptClient, error) {
	return &fakeExecuteScriptClient{responses: vs.responses, err: vs.err}, nil
}

func (vs *fakeVizierServiceClient) HealthCheck(ctx context.Context, in *vizierpb.HealthCheckRequest, opts ...grpc.CallOption) (vizierpb.VizierService_HealthCheckClient, error) {
	return nil, errors.New("Not implemented")
}

func (vs *fakeVizierServiceClient) GenerateOTelScript(ctx context.Context, req *vizierpb.GenerateOTelScriptRequest, opts ...grpc.CallOption) (*vizierpb.GenerateOTelScriptResponse, error) {
	return nil, errors.New("Not implemented")
}

type FuncMatcher[T any] struct {
	name    string
	matches func(x T) bool
}

func NewCustomMatcher[T any](name string, matches func(x T) bool) *FuncMatcher[T] {
	return &FuncMatcher[T]{name, matches}
}

func (f *FuncMatcher[T]) Matches(x interface{}) bool {
	t, ok := x.(T)
	return ok && f.matches(t)
}

func (f *FuncMatcher[T]) String() string {
	return f.name
}

func mockUpdatesCh() chan *cvmsgspb.CronScriptUpdate {
	return make(chan *cvmsgspb.CronScriptUpdate)
}
