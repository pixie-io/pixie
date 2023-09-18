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

package tracepoint_test

import (
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	mock_agent "px.dev/pixie/src/vizier/services/metadata/controllers/agent/mock"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	mock_tracepoint "px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint/mock"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
)

func TestCreateTracepoint(t *testing.T) {
	tests := []struct {
		name                    string
		originalTracepoint      *logicalpb.TracepointDeployment
		originalTracepointState statuspb.LifeCycleState
		newTracepoint           *logicalpb.TracepointDeployment
		expectError             bool
		expectOldUpdated        bool
		expectTTLUpdateOnly     bool
	}{
		{
			name:               "new_tracepoint",
			originalTracepoint: nil,
			newTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError: false,
		},
		{
			name: "existing tracepoint, match",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			originalTracepointState: statuspb.RUNNING_STATE,
			newTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectTTLUpdateOnly: true,
		},
		{
			name: "existing tracepoint, not exactly same (1)",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc"},
								},
							},
						},
					},
				},
			},
			originalTracepointState: statuspb.RUNNING_STATE,
			newTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectOldUpdated: true,
		},
		{
			name:                    "existing tracepoint, not exactly same (2)",
			originalTracepointState: statuspb.RUNNING_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probe: &logicalpb.Probe{
								Name: "test",
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectOldUpdated: true,
		},
		{
			name:                    "existing terminated tracepoint",
			originalTracepointState: statuspb.TERMINATED_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probe: &logicalpb.Probe{
								Name: "test",
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

			origID := uuid.Must(uuid.NewV4())

			if test.originalTracepoint == nil {
				mockTracepointStore.
					EXPECT().
					GetTracepointsWithNames([]string{"test_tracepoint"}).
					Return([]*uuid.UUID{nil}, nil)
			} else {
				mockTracepointStore.
					EXPECT().
					GetTracepointsWithNames([]string{"test_tracepoint"}).
					Return([]*uuid.UUID{&origID}, nil)
				mockTracepointStore.
					EXPECT().
					GetTracepoint(origID).
					Return(&storepb.TracepointInfo{
						ExpectedState: test.originalTracepointState,
						Tracepoint:    test.originalTracepoint,
					}, nil)
			}

			if test.expectTTLUpdateOnly {
				mockTracepointStore.
					EXPECT().
					SetTracepointTTL(origID, time.Second*5)
			}

			if test.expectOldUpdated {
				mockTracepointStore.
					EXPECT().
					DeleteTracepointTTLs([]uuid.UUID{origID}).
					Return(nil)
			}

			var newID uuid.UUID

			if !test.expectError && !test.expectTTLUpdateOnly {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint(gomock.Any(), gomock.Any()).
					DoAndReturn(func(id uuid.UUID, tpInfo *storepb.TracepointInfo) error {
						newID = id
						assert.Equal(t, &storepb.TracepointInfo{
							Tracepoint:    test.newTracepoint,
							Name:          "test_tracepoint",
							ID:            utils.ProtoFromUUID(id),
							ExpectedState: statuspb.RUNNING_STATE,
						}, tpInfo)
						return nil
					})

				mockTracepointStore.
					EXPECT().
					SetTracepointWithName("test_tracepoint", gomock.Any()).
					DoAndReturn(func(name string, id uuid.UUID) error {
						assert.Equal(t, newID, id)
						return nil
					})

				mockTracepointStore.
					EXPECT().
					SetTracepointTTL(gomock.Any(), time.Second*5).
					DoAndReturn(func(id uuid.UUID, ttl time.Duration) error {
						assert.Equal(t, newID, id)
						return nil
					})
			}

			mockAgtMgr := mock_agent.NewMockManager(ctrl)
			tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
			defer tracepointMgr.Close()

			actualTpID, err := tracepointMgr.CreateTracepoint("test_tracepoint", test.newTracepoint, time.Second*5)
			if test.expectError || test.expectTTLUpdateOnly {
				assert.Equal(t, tracepoint.ErrTracepointAlreadyExists, err)
			} else {
				require.NoError(t, err)
				assert.Equal(t, &newID, actualTpID)
			}
		})
	}
}

func TestGetTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tID1 := uuid.Must(uuid.NewV4())
	tID2 := uuid.Must(uuid.NewV4())
	expectedTracepointInfo := []*storepb.TracepointInfo{
		{
			ID: utils.ProtoFromUUID(tID1),
		},
		{
			ID: utils.ProtoFromUUID(tID2),
		},
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetAllTracepoints()
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tID1 := uuid.Must(uuid.NewV4())
	expectedTracepointInfo := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tID1),
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tID1).
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetTracepointInfo(tID1)
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointStates(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tID1 := uuid.Must(uuid.NewV4())
	expectedTracepointStatus1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tID1),
		AgentID: utils.ProtoFromUUID(agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.Must(uuid.NewV4())
	expectedTracepointStatus2 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tID1),
		AgentID: utils.ProtoFromUUID(agentUUID2),
		State:   statuspb.PENDING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tID1).
		Return([]*storepb.AgentTracepointStatus{expectedTracepointStatus1, expectedTracepointStatus2}, nil)

	tracepoints, err := tracepointMgr.GetTracepointStates(tID1)
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointStatus1, tracepoints[0])
	assert.Equal(t, expectedTracepointStatus2, tracepoints[1])
}

func TestRegisterTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tracepointDeployment := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			{
				TableName: "programFrom5.10.0To5.18.0",
				// In the future, we can test more selector types.
				Selectors: []*logicalpb.TracepointSelector{
					{
						SelectorType: logicalpb.MIN_KERNEL,
						Value:        "5.10.0",
					},
					{
						SelectorType: logicalpb.MAX_KERNEL,
						Value:        "5.18.0",
					},
				},
			},
			{
				TableName: "programUpTo5.18.0",
				Selectors: []*logicalpb.TracepointSelector{
					{
						SelectorType: logicalpb.MAX_KERNEL,
						Value:        "5.18.0",
					},
				},
			},
			{
				TableName: "programFrom5.19.0",
				Selectors: []*logicalpb.TracepointSelector{
					{
						SelectorType: logicalpb.MIN_KERNEL,
						Value:        "5.19.0",
					},
				},
			},
			{
				TableName: "programHostName",
				Selectors: []*logicalpb.TracepointSelector{
					{
						SelectorType: logicalpb.HOST_NAME,
						Value:        "myHostName",
					},
				},
			},
		},
	}

	agentUUID1 := uuid.Must(uuid.NewV4())
	agentUUID2 := uuid.Must(uuid.NewV4())
	agentUUID3 := uuid.Must(uuid.NewV4())
	upb1 := utils.ProtoFromUUID(agentUUID1)
	upb2 := utils.ProtoFromUUID(agentUUID2)
	upb3 := utils.ProtoFromUUID(agentUUID3)
	mockAgents := []*agentpb.Agent{
		// Should match programUpTo5.18.0 and programFrom5.10.0To5.18.0
		{
			Info: &agentpb.AgentInfo{
				HostInfo: &agentpb.HostInfo{
					Hostname: "localhost",
					HostIP:   "127.0.0.4",
					Kernel: &agentpb.KernelVersion{
						Version:  5,
						MajorRev: 18,
						MinorRev: 0,
					},
				},
				AgentID: upb1,
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: true,
				},
			},
		},
		// Should match programFrom5.19.0
		{
			Info: &agentpb.AgentInfo{
				HostInfo: &agentpb.HostInfo{
					Hostname: "localhost",
					HostIP:   "127.0.0.4",
					Kernel: &agentpb.KernelVersion{
						Version:  5,
						MajorRev: 19,
						MinorRev: 0,
					},
				},
				AgentID: upb2,
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: true,
				},
			},
		},
		// Should match programHostName
		{
			Info: &agentpb.AgentInfo{
				HostInfo: &agentpb.HostInfo{
					Hostname: "myHostName",
					HostIP:   "127.0.0.4",
				},
				AgentID: upb3,
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: true,
				},
			},
		},
	}

	tracepointID := uuid.Must(uuid.NewV4())
	// Expected message to be sent to agent1. Should match programUpTo5.18.0 and programFrom5.10.0To5.18.0.
	deploymentAgent1 := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			tracepointDeployment.Programs[0],
			tracepointDeployment.Programs[1],
		},
	}
	expectedTracepointReq1 := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_TracepointMessage{
			TracepointMessage: &messagespb.TracepointMessage{
				Msg: &messagespb.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messagespb.RegisterTracepointRequest{
						TracepointDeployment: deploymentAgent1,
						ID:                   utils.ProtoFromUUID(tracepointID),
					},
				},
			},
		},
	}
	// Expected message to be sent to agent2. Should match programFrom5.19.0.
	deploymentAgent2 := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			tracepointDeployment.Programs[2],
		},
	}
	expectedTracepointReq2 := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_TracepointMessage{
			TracepointMessage: &messagespb.TracepointMessage{
				Msg: &messagespb.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messagespb.RegisterTracepointRequest{
						TracepointDeployment: deploymentAgent2,
						ID:                   utils.ProtoFromUUID(tracepointID),
					},
				},
			},
		},
	}

	// Expected message to be sent to agent2. Should match programFrom5.19.0.
	deploymentAgent3 := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			tracepointDeployment.Programs[3],
		},
	}
	expectedTracepointReq3 := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_TracepointMessage{
			TracepointMessage: &messagespb.TracepointMessage{
				Msg: &messagespb.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messagespb.RegisterTracepointRequest{
						TracepointDeployment: deploymentAgent3,
						ID:                   utils.ProtoFromUUID(tracepointID),
					},
				},
			},
		},
	}

	// Serialize tracepoint request proto into byte slice to compare with the actual message sent to agents.
	msg1, err := expectedTracepointReq1.Marshal()
	if err != nil {
		t.Fatal(err)
	}
	msg2, err := expectedTracepointReq2.Marshal()
	if err != nil {
		t.Fatal(err)
	}
	msg3, err := expectedTracepointReq3.Marshal()
	if err != nil {
		t.Fatal(err)
	}

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{agentUUID1}, msg1).
		Return(nil)

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{agentUUID2}, msg2).
		Return(nil)

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{agentUUID3}, msg3).
		Return(nil)

	err = tracepointMgr.RegisterTracepoint(mockAgents, tracepointID, tracepointDeployment)
	require.NoError(t, err)
}

func TestUpdateAgentTracepointStatus(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())
	expectedTracepointState := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(expectedTracepointState).
		Return(nil)

	err := tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(tpID), utils.ProtoFromUUID(agentUUID1), statuspb.RUNNING_STATE, nil)
	require.NoError(t, err)
}

func TestUpdateAgentTracepointStatus_Terminated(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())
	agentUUID2 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tpID).
		Return([]*storepb.AgentTracepointStatus{
			{AgentID: utils.ProtoFromUUID(agentUUID1), State: statuspb.TERMINATED_STATE},
			{AgentID: utils.ProtoFromUUID(agentUUID2), State: statuspb.RUNNING_STATE},
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepoint(tpID).
		Return(nil)

	err := tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(tpID), utils.ProtoFromUUID(agentUUID2), statuspb.TERMINATED_STATE, nil)
	require.NoError(t, err)
}

func TestTTLExpiration(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	tpID1 := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())
	tpID3 := uuid.Must(uuid.NewV4())
	tpID4 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return([]*storepb.TracepointInfo{
			{
				ID: utils.ProtoFromUUID(tpID1),
			},
			{
				ID: utils.ProtoFromUUID(tpID2),
			},
			{
				ID: utils.ProtoFromUUID(tpID3),
			},
			{
				ID:            utils.ProtoFromUUID(tpID4),
				ExpectedState: statuspb.TERMINATED_STATE,
			},
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepointTTLs().
		Return([]uuid.UUID{
			tpID1,
			tpID3,
			tpID4,
		}, []time.Time{
			time.Now().Add(1 * time.Hour),
			time.Now().Add(-1 * time.Minute),
			time.Now().Add(-1 * time.Hour),
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tpID2).
		Return(&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(tpID2),
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tpID3).
		Return(&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(tpID3),
		}, nil)

	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(tpID2, &storepb.TracepointInfo{ID: utils.ProtoFromUUID(tpID2), ExpectedState: statuspb.TERMINATED_STATE}).
		Return(nil)

	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(tpID3, &storepb.TracepointInfo{ID: utils.ProtoFromUUID(tpID3), ExpectedState: statuspb.TERMINATED_STATE}).
		Return(nil)

	var wg sync.WaitGroup
	wg.Add(2)

	var seenDeletions []string
	msgHandler := func(msg []byte) error {
		vzMsg := &messagespb.VizierMessage{}
		err := proto.Unmarshal(msg, vzMsg)
		require.NoError(t, err)
		req := vzMsg.GetTracepointMessage().GetRemoveTracepointRequest()
		assert.NotNil(t, req)
		seenDeletions = append(seenDeletions, utils.ProtoToUUIDStr(req.ID))

		wg.Done()
		return nil
	}

	mockAgtMgr.
		EXPECT().
		MessageActiveAgents(gomock.Any()).
		Times(2).
		DoAndReturn(msgHandler)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 25*time.Millisecond)
	defer tracepointMgr.Close()

	wg.Wait()
	assert.Contains(t, seenDeletions, tpID2.String())
	assert.Contains(t, seenDeletions, tpID3.String())
}

func TestUpdateAgentTracepointStatus_RemoveTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tpID1 := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepointsWithNames([]string{"test1", "test2"}).
		Return([]*uuid.UUID{
			&tpID1, &tpID2,
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepointTTLs([]uuid.UUID{tpID1, tpID2}).
		Return(nil)

	err := tracepointMgr.RemoveTracepoints([]string{"test1", "test2"})
	require.NoError(t, err)
}
