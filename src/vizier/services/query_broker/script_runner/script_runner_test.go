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
	"io"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vizierpb"
	mock_vizierpb "px.dev/pixie/src/api/proto/vizierpb/mock"
	"px.dev/pixie/src/carnot/planner/compilerpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

func TestScriptRunner_SyncScripts(t *testing.T) {
	t.Run("returns an error when the script source fails", func(t *testing.T) {
		err := errors.New("failed to initialize scripts")
		sr := New(nil, nil, "test", errorSource(err))

		require.Error(t, sr.SyncScripts())
	})

	t.Run("returns when we call Stop", func(t *testing.T) {
		sr := New(nil, nil, "test", dummySource())
		stopped := make(chan struct{}, 1)

		go func() {
			require.NoError(t, sr.SyncScripts())
			stopped <- struct{}{}
		}()

		requireNoReceive(t, stopped, time.Millisecond)

		sr.Stop()

		requireReceiveWithin(t, stopped, 10*time.Millisecond)
	})

	t.Run("stops updates when we stop the ScriptRunner", func(t *testing.T) {
		stopped := make(chan struct{}, 1)
		sr := New(nil, nil, "test", fakeSource(nil, func() {
			stopped <- struct{}{}
		}, nil))

		go func() {
			require.NoError(t, sr.SyncScripts())
		}()

		requireNoReceive(t, stopped, time.Millisecond)

		sr.Stop()

		requireReceiveWithin(t, stopped, 10*time.Millisecond)
	})

	t.Run("runs initial scripts on an interval", func(t *testing.T) {
		const scriptID = "223e4567-e89b-12d3-a456-426655440002"
		ctrl := gomock.NewController(t)
		mvs := mock_vizierpb.NewMockVizierServiceClient(ctrl)
		mvsExecuteScriptClient := mock_vizierpb.NewMockVizierService_ExecuteScriptClient(ctrl)
		mvsExecuteScriptClient.EXPECT().
			Recv().
			Return(nil, io.EOF).
			Times(1)

		scriptExecuted := make(chan struct{}, 1)
		mvs.EXPECT().
			ExecuteScript(
				gomock.Not(gomock.Nil()),
				NewCustomMatcher(
					"an ExecuteScriptRequest with the proper QueryName",
					func(req *vizierpb.ExecuteScriptRequest) bool {
						return req.GetQueryName() == "cron_"+scriptID
					},
				),
			).
			Do(func(_ context.Context, _ *vizierpb.ExecuteScriptRequest) { scriptExecuted <- struct{}{} }).
			Return(mvsExecuteScriptClient, nil).
			Times(1)

		fcs := &fakeCronStore{scripts: map[uuid.UUID]*cvmsgspb.CronScript{}}
		source := fakeSource(map[string]*cvmsgspb.CronScript{
			scriptID: {
				ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
				Script:     "px.display()",
				Configs:    "otelEndpointConfig: {url: example.com}",
				FrequencyS: 1,
			},
		}, func() {}, nil)
		sr := New(fcs, mvs, "test", source)

		go func() {
			require.NoError(t, sr.SyncScripts())
		}()
		defer sr.Stop()

		requireReceiveWithin(t, scriptExecuted, 10*time.Second)
	})

	t.Run("stops executing scripts when we stop the ScriptRunner", func(t *testing.T) {
		const scriptID = "223e4567-e89b-12d3-a456-426655440002"
		ctrl := gomock.NewController(t)
		scriptExecuted := make(chan struct{}, 1)
		mvsExecuteScriptClient := mock_vizierpb.NewMockVizierService_ExecuteScriptClient(ctrl)
		mvsExecuteScriptClient.EXPECT().
			Recv().
			Return(nil, io.EOF).
			Times(1)

		mvs := mock_vizierpb.NewMockVizierServiceClient(ctrl)
		mvs.EXPECT().
			ExecuteScript(
				gomock.Any(),
				gomock.Any(),
			).
			Do(func(_ context.Context, _ *vizierpb.ExecuteScriptRequest) { scriptExecuted <- struct{}{} }).
			Return(mvsExecuteScriptClient, nil).
			Times(1)

		fcs := &fakeCronStore{scripts: map[uuid.UUID]*cvmsgspb.CronScript{}}
		source := fakeSource(map[string]*cvmsgspb.CronScript{
			scriptID: {
				ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
				Script:     "px.display()",
				Configs:    "otelEndpointConfig: {url: example.com}",
				FrequencyS: 1,
			},
		}, func() {}, nil)
		sr := New(fcs, mvs, "test", source)

		go func() {
			require.NoError(t, sr.SyncScripts())
		}()

		requireReceiveWithin(t, scriptExecuted, 10*time.Second)

		sr.Stop()

		requireNoReceive(t, scriptExecuted, 1100*time.Millisecond)
	})
}

func TestScriptRunner_ScriptUpdates(t *testing.T) {
	const scriptID = "223e4567-e89b-12d3-a456-426655440002"
	tests := []struct {
		name     string
		queryStr string
		updates  []*cvmsgspb.CronScriptUpdate
	}{
		{
			name:     "tracks new cron scripts",
			queryStr: "updated script",
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
								Script:     "updated script",
								Configs:    "otelEndpointConfig: {url: example.com}",
								FrequencyS: 1,
							},
						},
					},
					Timestamp: 1,
				},
			},
		},
		{
			name:     "tracks updates to cron scripts",
			queryStr: "second updated script",
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
								Script:     "updated script",
								Configs:    "otelEndpointConfig: {url: example.com}",
								FrequencyS: 1,
							},
						},
					},
					Timestamp: 1,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_UpsertReq{
						UpsertReq: &cvmsgspb.RegisterOrUpdateCronScriptRequest{
							Script: &cvmsgspb.CronScript{
								ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
								Script:     "second updated script",
								Configs:    "otelEndpointConfig: {url: example.com}",
								FrequencyS: 1,
							},
						},
					},
					Timestamp: 2,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			mvsExecuteScriptClient := mock_vizierpb.NewMockVizierService_ExecuteScriptClient(ctrl)
			mvsExecuteScriptClient.EXPECT().
				Recv().
				Return(nil, io.EOF).
				Times(1)

			scriptExecuted := make(chan struct{}, 1)
			mvs := mock_vizierpb.NewMockVizierServiceClient(ctrl)
			mvs.EXPECT().
				ExecuteScript(
					gomock.Not(gomock.Nil()),
					NewCustomMatcher(
						"an ExecuteScriptRequest with the proper QueryName amd QueryStr",
						func(req *vizierpb.ExecuteScriptRequest) bool {
							return req.GetQueryName() == "cron_"+scriptID && req.GetQueryStr() == test.queryStr
						},
					),
				).
				Do(func(_ context.Context, _ *vizierpb.ExecuteScriptRequest) { scriptExecuted <- struct{}{} }).
				Return(mvsExecuteScriptClient, nil).
				Times(1)

			fcs := &fakeCronStore{scripts: map[uuid.UUID]*cvmsgspb.CronScript{}}
			source := &TestSource{
				stopDelegate: func() {},
				startDelegate: func(_ context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
					for _, update := range test.updates {
						updatesCh <- update
					}
					return map[string]*cvmsgspb.CronScript{}, nil
				},
			}
			sr := New(fcs, mvs, "test", source)
			defer sr.Stop()

			go func() {
				require.NoError(t, sr.SyncScripts())
			}()

			requireReceiveWithin(t, scriptExecuted, 10*time.Second)
		})
	}
}

func TestScriptRunner_ScriptDeletes(t *testing.T) {
	const scriptID = "223e4567-e89b-12d3-a456-426655440002"
	const nonExistentScriptID = "223e4567-e89b-12d3-a456-426655440001"
	tests := []struct {
		name     string
		queryStr string
		updates  []*cvmsgspb.CronScriptUpdate
	}{
		{
			name: "tracks deletes to cron scripts",
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil(scriptID),
						},
					},
					Timestamp: 1,
				},
			},
		},
		{
			name: "can delete non-existent runners",
			updates: []*cvmsgspb.CronScriptUpdate{
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil(nonExistentScriptID),
						},
					},
					Timestamp: 1,
				},
				{
					Msg: &cvmsgspb.CronScriptUpdate_DeleteReq{
						DeleteReq: &cvmsgspb.DeleteCronScriptRequest{
							ScriptID: utils.ProtoFromUUIDStrOrNil(scriptID),
						},
					},
					Timestamp: 1,
				},
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			mvs := mock_vizierpb.NewMockVizierServiceClient(ctrl)
			scriptExecuted := make(chan struct{}, 1)
			mvs.EXPECT().
				ExecuteScript(
					gomock.Any(),
					gomock.Any(),
				).
				Do(func(_ context.Context, _ *vizierpb.ExecuteScriptRequest) { scriptExecuted <- struct{}{} }).
				Times(0)

			fcs := &fakeCronStore{scripts: map[uuid.UUID]*cvmsgspb.CronScript{}}

			source := &TestSource{
				startDelegate: func(baseCtx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error) {
					for _, update := range test.updates {
						updatesCh <- update
					}
					return map[string]*cvmsgspb.CronScript{
						scriptID: {
							ID:         utils.ProtoFromUUIDStrOrNil(scriptID),
							Script:     "initial script",
							Configs:    "otelEndpointConfig: {url: example.com}",
							FrequencyS: 1,
						},
					}, nil
				},
				stopDelegate: func() {},
			}
			sr := New(fcs, mvs, "test", source)
			defer sr.Stop()

			go func() {
				require.NoError(t, sr.SyncScripts())
			}()

			requireNoReceive(t, scriptExecuted, 1100*time.Millisecond)
			sr.runnerMapMu.Lock()
			defer sr.runnerMapMu.Unlock()
			require.Empty(t, sr.runnerMap)
		})
	}
}

func TestScriptRunner_StoreResults(t *testing.T) {
	marshalMust := func(a *types.Any, _ error) *types.Any {
		return a
	}
	tests := []struct {
		name                string
		execScriptResponses []*vizierpb.ExecuteScriptResponse
		// We only test the Result part.
		expectedExecutionResult *metadatapb.RecordExecutionResultRequest
		err                     error
	}{
		{
			name: "forwards exec stats",
			execScriptResponses: []*vizierpb.ExecuteScriptResponse{
				{
					Result: &vizierpb.ExecuteScriptResponse_Data{
						Data: &vizierpb.QueryData{
							ExecutionStats: &vizierpb.QueryExecutionStats{
								Timing: &vizierpb.QueryTimingInfo{
									ExecutionTimeNs:   123,
									CompilationTimeNs: 456,
								},
								RecordsProcessed: 999,
								BytesProcessed:   1000,
							},
						},
					},
				},
			},
			expectedExecutionResult: &metadatapb.RecordExecutionResultRequest{
				Result: &metadatapb.RecordExecutionResultRequest_ExecutionStats{
					ExecutionStats: &metadatapb.ExecutionStats{
						ExecutionTimeNs:   123,
						CompilationTimeNs: 456,
						RecordsProcessed:  999,
						BytesProcessed:    1000,
					},
				},
			},
		},
		{
			name: "handles non-compiler error",
			execScriptResponses: []*vizierpb.ExecuteScriptResponse{
				{
					Status: &vizierpb.Status{
						Code:    3, // INVALID_ARGUMENT
						Message: "Invalid",
					},
				},
			},
			expectedExecutionResult: &metadatapb.RecordExecutionResultRequest{
				Result: &metadatapb.RecordExecutionResultRequest_Error{
					Error: &statuspb.Status{
						ErrCode: statuspb.INVALID_ARGUMENT,
						Msg:     "Invalid",
					},
				},
			},
		},
		{
			name: "handles compiler error",
			execScriptResponses: []*vizierpb.ExecuteScriptResponse{
				{
					Status: &vizierpb.Status{
						Code: 3, // INVALID_ARGUMENT
						ErrorDetails: []*vizierpb.ErrorDetails{
							{
								Error: &vizierpb.ErrorDetails_CompilerError{
									CompilerError: &vizierpb.CompilerError{
										Message: "syntax error",
										Line:    123,
										Column:  456,
									},
								},
							},
						},
					},
				},
			},
			expectedExecutionResult: &metadatapb.RecordExecutionResultRequest{
				Result: &metadatapb.RecordExecutionResultRequest_Error{
					Error: &statuspb.Status{
						ErrCode: statuspb.INVALID_ARGUMENT,
						Context: marshalMust(types.MarshalAny(&compilerpb.CompilerErrorGroup{
							Errors: []*compilerpb.CompilerError{
								{
									Error: &compilerpb.CompilerError_LineColError{
										LineColError: &compilerpb.LineColError{
											Message: "syntax error",
											Line:    123,
											Column:  456,
										},
									},
								},
							},
						})),
					},
				},
			},
		},
		{
			name: "handles grpc error",
			err:  status.New(codes.InvalidArgument, "Invalid").Err(),
			expectedExecutionResult: &metadatapb.RecordExecutionResultRequest{
				Result: &metadatapb.RecordExecutionResultRequest_Error{
					Error: &statuspb.Status{
						ErrCode: statuspb.INVALID_ARGUMENT,
						Msg:     "Invalid",
					},
				},
			},
		},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			receivedResultRequestCh := make(chan *metadatapb.RecordExecutionResultRequest)
			fcs := &fakeCronStore{scripts: make(map[uuid.UUID]*cvmsgspb.CronScript), receivedResultRequestCh: receivedResultRequestCh}

			script := &cvmsgspb.CronScript{
				ID:         utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
				Script:     "px.display()",
				Configs:    "otelEndpointConfig: {url: example.com}",
				FrequencyS: 1,
			}

			id := uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")
			fvs := &fakeVizierServiceClient{responses: test.execScriptResponses, err: test.err}
			Runner := newRunner(script, fvs, "test", id, fcs)
			Runner.start()

			result := requireReceiveWithin(t, receivedResultRequestCh, 10*time.Second)

			Runner.stop()
			require.NotNil(t, result, "Failed to receive a valid result")

			require.Equal(t, utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"), result.ScriptID)
			require.Equal(t, test.expectedExecutionResult.GetError(), result.GetError())
			require.Equal(t, test.expectedExecutionResult.GetExecutionStats(), result.GetExecutionStats())
		})
	}
}
