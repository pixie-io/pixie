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
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/controllers"
	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	mock_agent "px.dev/pixie/src/vizier/services/metadata/controllers/agent/mock"
	"px.dev/pixie/src/vizier/services/metadata/controllers/testutils"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	mock_tracepoint "px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint/mock"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
)

func assertSendMessageUncalled(t *testing.T) controllers.SendMessageFn {
	return func(topic string, b []byte) error {
		assert.Fail(t, "SendMsg shouldn't be called")
		return nil
	}
}

func assertSendMessageCalledWith(t *testing.T, expTopic string, expMsg messagespb.VizierMessage) controllers.SendMessageFn {
	return func(topic string, b []byte) error {
		msg := &messagespb.VizierMessage{}
		err := proto.Unmarshal(b, msg)

		require.NoError(t, err)
		assert.Equal(t, expMsg, *msg)
		assert.Equal(t, expTopic, topic)
		return nil
	}
}

func setup(t *testing.T, sendMsgFn controllers.SendMessageFn) (*controllers.AgentTopicListener, *mock_agent.MockManager, *mock_tracepoint.MockStore, func()) {
	ctrl := gomock.NewController(t)

	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	agentInfo := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.UnhealthyKelvinAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for unhealthy kelvin agent")
	}

	// Load some existing agents.
	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return([]*agentpb.Agent{agentInfo}, nil)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	atl, _ := controllers.NewAgentTopicListener(mockAgtMgr, tracepointMgr, sendMsgFn)

	cleanup := func() {
		ctrl.Finish()
		tracepointMgr.Close()
	}

	return atl, mockAgtMgr, mockTracepointStore, cleanup
}

func TestAgentRegisterRequest(t *testing.T) {
	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	sendMsg := assertSendMessageCalledWith(t, "Agent/"+testutils.NewAgentUUID,
		messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_RegisterAgentResponse{
				RegisterAgentResponse: &messagespb.RegisterAgentResponse{
					ASID: 1,
				},
			},
		})

	// Set up mock.
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, mockTracepointStore, cleanup := setup(t, sendMsg)
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: utils.ProtoFromUUID(u),
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
	}

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&agent.HostnameIPPair{Hostname: "", IP: "127.0.0.1"}).
		Return("", nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		DoAndReturn(func() ([]*storepb.TracepointInfo, error) {
			wg.Done()
			return nil, nil
		})

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetRegisterAgentRequest().Info.Capabilities = &agentpb.AgentCapabilities{
		CollectsData: true,
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	now := time.Now().UnixNano()
	mockAgtMgr.
		EXPECT().
		RegisterAgent(gomock.Any()).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			assert.Greater(t, info.LastHeartbeatNS, now)
			assert.Greater(t, info.CreateTimeNS, now)
			info.LastHeartbeatNS = 0
			info.CreateTimeNS = 0
			assert.Equal(t, agentInfo, info)
			return uint32(1), nil
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)

	defer wg.Wait()
}

func TestKelvinRegisterRequest(t *testing.T) {
	u, err := uuid.FromString(testutils.KelvinAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	sendMsg := assertSendMessageCalledWith(t, "Agent/"+testutils.KelvinAgentUUID,
		messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_RegisterAgentResponse{
				RegisterAgentResponse: &messagespb.RegisterAgentResponse{
					ASID: 1,
				},
			},
		})

	// Set up mock.
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, mockTracepointStore, cleanup := setup(t, sendMsg)
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: utils.ProtoFromUUID(u),
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: false,
			},
		},
	}

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&agent.HostnameIPPair{Hostname: "test-host", IP: "127.0.0.1"}).
		Return("", nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		DoAndReturn(func() ([]*storepb.TracepointInfo, error) {
			wg.Done()
			return nil, nil
		})

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterKelvinRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	now := time.Now().UnixNano()
	mockAgtMgr.
		EXPECT().
		RegisterAgent(gomock.Any()).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			assert.Greater(t, info.LastHeartbeatNS, now)
			assert.Greater(t, info.CreateTimeNS, now)
			info.LastHeartbeatNS = 0
			info.CreateTimeNS = 0
			assert.Equal(t, agentInfo, info)
			return uint32(1), nil
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)

	defer wg.Wait()
}

func TestAgentReRegisterRequest(t *testing.T) {
	u, err := uuid.FromString(testutils.PurgedAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	sendMsg := assertSendMessageCalledWith(t, "Agent/"+testutils.PurgedAgentUUID,
		messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_RegisterAgentResponse{
				RegisterAgentResponse: &messagespb.RegisterAgentResponse{
					ASID: 159,
				},
			},
		})

	// Set up mock.
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, mockTracepointStore, cleanup := setup(t, sendMsg)
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "purged",
				HostIP:   "127.0.10.1",
			},
			AgentID: utils.ProtoFromUUID(u),
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		ASID: 159,
	}

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&agent.HostnameIPPair{Hostname: "", IP: "127.0.10.1"}).
		Return("", nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		DoAndReturn(func() ([]*storepb.TracepointInfo, error) {
			wg.Done()
			return nil, nil
		})

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.ReregisterPurgedAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetRegisterAgentRequest().Info.Capabilities = &agentpb.AgentCapabilities{
		CollectsData: true,
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	now := time.Now().UnixNano()
	mockAgtMgr.
		EXPECT().
		RegisterAgent(gomock.Any()).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			assert.Greater(t, info.LastHeartbeatNS, now)
			assert.Greater(t, info.CreateTimeNS, now)
			info.LastHeartbeatNS = 0
			info.CreateTimeNS = 0
			assert.Equal(t, agentInfo, info)
			return agentInfo.ASID, nil
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)

	defer wg.Wait()
}

func TestAgentRegisterRequestInvalidUUID(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, assertSendMessageUncalled(t))
	defer cleanup()

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidRegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)
}

func TestAgentCreateFailed(t *testing.T) {
	var wg sync.WaitGroup
	atl, mockAgtMgr, _, cleanup := setup(t, assertSendMessageUncalled(t))
	defer cleanup()

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetRegisterAgentRequest().Info.Capabilities = &agentpb.AgentCapabilities{
		CollectsData: false,
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&agent.HostnameIPPair{Hostname: "test-host", IP: "127.0.0.1"}).
		Return("", nil)

	wg.Add(1)
	mockAgtMgr.
		EXPECT().
		RegisterAgent(gomock.Any()).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			wg.Done()
			return uint32(0), errors.New("could not create agent")
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)

	wg.Wait()
}

func TestAgentHeartbeat(t *testing.T) {
	// Create request and expected response protos.
	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetHeartbeat().AgentID = utils.ProtoFromUUIDStrOrNil(testutils.UnhealthyKelvinAgentUUID)
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	resp := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, resp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	now := time.Now().UnixNano()

	// Set up mock.
	var wg sync.WaitGroup
	atl, mockAgtMgr, _, cleanup := setup(t, func(topic string, b []byte) error {
		msg := messagespb.VizierMessage{}
		if err := proto.Unmarshal(b, &msg); err != nil {
			t.Fatal("Cannot Unmarshal protobuf.")
		}
		// Don't assert on exact timing.
		assert.Greater(t, msg.Msg.(*messagespb.VizierMessage_HeartbeatAck).HeartbeatAck.Time, now)
		msg.Msg.(*messagespb.VizierMessage_HeartbeatAck).HeartbeatAck.Time = 0
		assert.Equal(t, *resp, msg)
		assert.Equal(t, "Agent/"+testutils.UnhealthyKelvinAgentUUID, topic)
		wg.Done()
		return nil
	})
	defer cleanup()

	mockAgtMgr.
		EXPECT().
		GetServiceCIDR().
		Return("10.64.4.0/22")

	mockAgtMgr.
		EXPECT().
		GetPodCIDRs().
		Return([]string{"10.64.4.0/21"})

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(uuid.FromStringOrNil(testutils.UnhealthyKelvinAgentUUID)).
		DoAndReturn(func(agentID uuid.UUID) error {
			return nil
		})

	createdProcesses := make([]*metadatapb.ProcessCreated, 1)
	createdProcesses[0] = &metadatapb.ProcessCreated{
		CID: "test",
	}
	agentUpdatePb := &messagespb.AgentUpdateInfo{
		ProcessCreated: createdProcesses,
	}

	mockAgtMgr.
		EXPECT().
		ApplyAgentUpdate(&agent.Update{AgentID: uuid.FromStringOrNil(testutils.UnhealthyKelvinAgentUUID), UpdateInfo: agentUpdatePb}).
		DoAndReturn(func(msg *agent.Update) error {
			wg.Done()
			return nil
		})

	wg.Add(2)
	defer wg.Wait()

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)
}

func TestAgentHeartbeat_Failed(t *testing.T) {
	sendMsg := assertSendMessageCalledWith(t, "Agent/"+testutils.UnhealthyKelvinAgentUUID,
		messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messagespb.HeartbeatNack{
					Reregister: true,
				},
			},
		})

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetHeartbeat().AgentID = utils.ProtoFromUUIDStrOrNil(testutils.UnhealthyKelvinAgentUUID)
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	// Set up mock.
	atl, mockAgtMgr, _, cleanup := setup(t, sendMsg)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(1)

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(uuid.FromStringOrNil(testutils.UnhealthyKelvinAgentUUID)).
		DoAndReturn(func(agentID uuid.UUID) error {
			wg.Done()
			return errors.New("Could not update heartbeat")
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)

	wg.Wait()
}

func TestEmptyMessage(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, assertSendMessageUncalled(t))
	defer cleanup()
	req := new(messagespb.VizierMessage)
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)
}

func TestUnhandledMessage(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, assertSendMessageUncalled(t))
	defer cleanup()

	req := new(messagespb.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)
	// Send update.
	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)
}

func TestAgentTracepointInfoUpdate(t *testing.T) {
	// Set up mock.
	atl, _, mockTracepointStore, cleanup := setup(t, assertSendMessageUncalled(t))
	defer cleanup()

	agentID := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(&storepb.AgentTracepointStatus{
			ID:      utils.ProtoFromUUID(tpID),
			AgentID: utils.ProtoFromUUID(agentID),
			State:   statuspb.RUNNING_STATE,
		}).
		Return(nil)

	req := &messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_TracepointMessage{
			TracepointMessage: &messagespb.TracepointMessage{
				Msg: &messagespb.TracepointMessage_TracepointInfoUpdate{
					TracepointInfoUpdate: &messagespb.TracepointInfoUpdate{
						ID:      utils.ProtoFromUUID(tpID),
						AgentID: utils.ProtoFromUUID(agentID),
						State:   statuspb.RUNNING_STATE,
					},
				},
			},
		},
	}
	reqPb, err := req.Marshal()
	require.NoError(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	require.NoError(t, err)
}

func TestAgentStop(t *testing.T) {
	u, err := uuid.FromString(testutils.NewAgentUUID)
	require.NoError(t, err)

	sendMsg := assertSendMessageCalledWith(t, "Agent/"+testutils.NewAgentUUID,
		messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messagespb.HeartbeatNack{
					Reregister: false,
				},
			},
		})

	// Set up mock.
	atl, _, _, cleanup := setup(t, sendMsg)
	defer cleanup()

	atl.StopAgent(u)
}
