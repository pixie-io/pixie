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

package controllers_test

import (
	"context"
	"fmt"
	"io"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/vizierpb"
	mock_vizierpb "px.dev/pixie/src/api/proto/vizierpb/mock"
	"px.dev/pixie/src/carnot/carnotpb"
	mock_carnotpb "px.dev/pixie/src/carnot/carnotpb/mock"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
	"px.dev/pixie/src/vizier/services/query_broker/querybrokerenv"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
)

type fakeQueryExecutor struct {
	ResultsToSend []*vizierpb.ExecuteScriptResponse
	RunError      error
	WaitError     error
	queryID       uuid.UUID

	consumer    controllers.QueryResultConsumer
	ReqReceived *vizierpb.ExecuteScriptRequest
}

func (q *fakeQueryExecutor) Run(ctx context.Context, req *vizierpb.ExecuteScriptRequest, consumer controllers.QueryResultConsumer) error {
	q.ReqReceived = req
	q.consumer = consumer
	return q.RunError
}

func (q *fakeQueryExecutor) Wait() error {
	if q.WaitError != nil {
		return q.WaitError
	}
	for _, result := range q.ResultsToSend {
		if err := q.consumer.Consume(result); err != nil {
			return err
		}
	}
	return nil
}

func (q *fakeQueryExecutor) QueryID() uuid.UUID {
	return q.queryID
}

func buildCheckHealthSuccessResponses(queryID uuid.UUID) []*vizierpb.ExecuteScriptResponse {
	fakeResult := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{
										[]byte("foo"),
									},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     true,
					Eos:     true,
				},
			},
		},
	}
	return []*vizierpb.ExecuteScriptResponse{fakeResult}
}

func buildCheckHealthEmptyResponses(queryID uuid.UUID) []*vizierpb.ExecuteScriptResponse {
	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{},
								},
							},
						},
					},
					NumRows: 0,
					Eow:     false,
					Eos:     false,
				},
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{},
								},
							},
						},
					},
					NumRows: 0,
					Eow:     true,
					Eos:     true,
				},
			},
		},
	}
	return []*vizierpb.ExecuteScriptResponse{fakeResult1, fakeResult2}
}

func buildCheckHealthTooManyResponses(queryID uuid.UUID) []*vizierpb.ExecuteScriptResponse {
	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{[]byte("foo")},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     false,
					Eos:     false,
				},
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{[]byte("bar")},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     true,
					Eos:     true,
				},
			},
		},
	}
	return []*vizierpb.ExecuteScriptResponse{fakeResult1, fakeResult2}
}

func TestCheckHealth(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())
	tests := []struct {
		Name        string
		Results     []*vizierpb.ExecuteScriptResponse
		RunError    error
		WaitError   error
		ExpectedErr error
	}{
		{
			Name:        "success",
			Results:     buildCheckHealthSuccessResponses(queryID),
			RunError:    nil,
			WaitError:   nil,
			ExpectedErr: nil,
		},
		{
			Name:        "no results",
			Results:     []*vizierpb.ExecuteScriptResponse{},
			RunError:    nil,
			WaitError:   nil,
			ExpectedErr: fmt.Errorf("results not returned on health check for query ID %s", queryID.String()),
		},
		{
			Name:        "empty results",
			Results:     buildCheckHealthEmptyResponses(queryID),
			RunError:    nil,
			WaitError:   nil,
			ExpectedErr: fmt.Errorf("results not returned on health check for query ID %s", queryID.String()),
		},
		{
			Name:        "too many results",
			Results:     buildCheckHealthTooManyResponses(queryID),
			RunError:    nil,
			WaitError:   nil,
			ExpectedErr: fmt.Errorf("bad results on healthcheck for query ID %s", queryID.String()),
		},
		{
			Name:        "run error",
			Results:     []*vizierpb.ExecuteScriptResponse{},
			RunError:    fmt.Errorf("an error"),
			WaitError:   nil,
			ExpectedErr: fmt.Errorf("an error"),
		},
		{
			Name:        "wait error",
			Results:     []*vizierpb.ExecuteScriptResponse{},
			RunError:    nil,
			WaitError:   fmt.Errorf("an error"),
			ExpectedErr: fmt.Errorf("an error"),
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			qe := &fakeQueryExecutor{
				ResultsToSend: test.Results,
				RunError:      test.RunError,
				WaitError:     test.WaitError,
				queryID:       queryID,
			}
			queryExecFactory := func(*controllers.Server, controllers.MutationExecFactory) controllers.QueryExecutor {
				return qe
			}

			dp := &fakeDataPrivacy{}
			s, err := controllers.NewServerWithForwarderAndPlanner(nil, nil, dp, nil, nil, nil, nil, nil, queryExecFactory)
			require.NoError(t, err)

			err = s.CheckHealth(context.Background())
			require.Equal(t, test.ExpectedErr, err)

			expectedReq := &vizierpb.ExecuteScriptRequest{
				QueryStr:  `import px; px.display(px.Version())`,
				QueryName: "healthcheck",
			}
			assert.Equal(t, expectedReq, qe.ReqReceived)
		})
	}
}

func buildExecuteScriptSuccessResponses(queryID uuid.UUID) []*vizierpb.ExecuteScriptResponse {
	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "execute_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{[]byte("foo")},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     false,
					Eos:     false,
				},
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "execute_unused",
					Cols: []*vizierpb.Column{
						{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
									Data: [][]byte{[]byte("bar")},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     true,
					Eos:     true,
				},
			},
		},
	}
	return []*vizierpb.ExecuteScriptResponse{fakeResult1, fakeResult2}
}

func TestExecuteScript(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())
	tests := []struct {
		Name        string
		Req         *vizierpb.ExecuteScriptRequest
		Results     []*vizierpb.ExecuteScriptResponse
		RunError    error
		WaitError   error
		SendError   error
		ExpectedErr error
	}{
		{
			Name:        "success",
			Req:         &vizierpb.ExecuteScriptRequest{QueryStr: "success"},
			Results:     buildExecuteScriptSuccessResponses(queryID),
			RunError:    nil,
			WaitError:   nil,
			SendError:   nil,
			ExpectedErr: nil,
		},
		{
			Name:        "run error",
			Req:         &vizierpb.ExecuteScriptRequest{QueryStr: "run error"},
			Results:     []*vizierpb.ExecuteScriptResponse{},
			RunError:    fmt.Errorf("an error"),
			WaitError:   nil,
			SendError:   nil,
			ExpectedErr: fmt.Errorf("an error"),
		},
		{
			Name:        "wait error",
			Req:         &vizierpb.ExecuteScriptRequest{QueryStr: "wait error"},
			Results:     []*vizierpb.ExecuteScriptResponse{},
			RunError:    nil,
			WaitError:   fmt.Errorf("an error"),
			SendError:   nil,
			ExpectedErr: fmt.Errorf("an error"),
		},
		{
			Name:        "send error",
			Req:         &vizierpb.ExecuteScriptRequest{QueryStr: "send error"},
			Results:     buildExecuteScriptSuccessResponses(queryID),
			RunError:    nil,
			WaitError:   nil,
			SendError:   fmt.Errorf("an error"),
			ExpectedErr: fmt.Errorf("an error"),
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			qe := &fakeQueryExecutor{
				ResultsToSend: test.Results,
				RunError:      test.RunError,
				WaitError:     test.WaitError,
				queryID:       queryID,
			}
			queryExecFactory := func(*controllers.Server, controllers.MutationExecFactory) controllers.QueryExecutor {
				return qe
			}

			dp := &fakeDataPrivacy{}
			s, err := controllers.NewServerWithForwarderAndPlanner(nil, nil, dp, nil, nil, nil, nil, nil, queryExecFactory)
			require.NoError(t, err)

			// Set up mocks.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			srv := mock_vizierpb.NewMockVizierService_ExecuteScriptServer(ctrl)
			auth := authcontext.New()
			ctx := authcontext.NewContext(context.Background(), auth)
			srv.EXPECT().Context().Return(ctx).AnyTimes()

			var resps []*vizierpb.ExecuteScriptResponse
			srv.EXPECT().
				Send(gomock.Any()).
				DoAndReturn(func(arg *vizierpb.ExecuteScriptResponse) error {
					resps = append(resps, arg)
					return test.SendError
				}).
				AnyTimes()

			err = s.ExecuteScript(test.Req, srv)
			require.Equal(t, test.ExpectedErr, err)

			if test.SendError != nil {
				// If we injected a send error then we won't get all the results so we skip those checks.
				return
			}
			assert.Equal(t, test.Req, qe.ReqReceived)

			require.Equal(t, len(test.Results), len(resps))
			i := 0
			for _, expectedResp := range test.Results {
				assert.Equal(t, expectedResp, resps[i])
				i++
			}
		})
	}
}

func TestTransferResultChunk_AgentStreamComplete(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	dp := &fakeDataPrivacy{}
	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, dp, &rf, nil, nil, nc, nil, nil)
	require.NoError(t, err)
	defer s.Close()

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}
	msg2 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_ExecutionAndTimingInfo{
			ExecutionAndTimingInfo: &carnotpb.TransferResultChunkRequest_QueryExecutionAndTimingInfo{
				ExecutionStats: &queryresultspb.QueryExecutionStats{
					Timing: &queryresultspb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(msg2, nil)
	srv.EXPECT().Recv().Return(nil, io.EOF)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: true,
		Message: "",
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 2, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
	assert.Equal(t, msg2, rf.ReceivedAgentResults[1])
}

func TestTransferResultChunk_AgentClosedPrematurely(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	dp := &fakeDataPrivacy{}
	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, dp, &rf, nil, nil, nc, nil, nil)
	require.NoError(t, err)
	defer s.Close()

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	sv.Eos = false

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	errorMsg := fmt.Sprintf(
		"Agent stream was unexpectedly closed for table %s of query %s before the results completed",
		"output_table_1", queryID.String(),
	)

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(nil, io.EOF)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: errorMsg,
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	// When the agent stream closes unexpectedly, the query will not be immediately cancelled.
	// Instead, it will be cancelled by the ResultForwarder after a timeout.
	// So the ClientStream will not be closed after this occurs.
	assert.False(t, rf.ClientStreamClosed)
	assert.Nil(t, rf.ClientStreamError)

	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_AgentStreamFailed(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	dp := &fakeDataPrivacy{}
	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, dp, &rf, nil, nil, nc, nil, nil)
	require.NoError(t, err)
	defer s.Close()

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(nil, fmt.Errorf("Agent error"))
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: "Agent error",
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.NotNil(t, rf.ClientStreamError)
	assert.Equal(t, rf.ClientStreamError.Error(), "Agent error")
	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_ClientStreamCancelled(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{
		ClientStreamClosed: true,
		ClientStreamError:  fmt.Errorf("An error"),
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	dp := &fakeDataPrivacy{}
	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, dp, &rf, nil, nil, nc, nil, nil)
	require.NoError(t, err)
	defer s.Close()

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: "An error",
	}).Return(nil)

	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.NotNil(t, rf.ClientStreamError)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))
}
