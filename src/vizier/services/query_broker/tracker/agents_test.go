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

package tracker_test

import (
	"sync"
	"testing"

	"github.com/golang/mock/gomock"

	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	mock_metadatapb "px.dev/pixie/src/vizier/services/metadata/metadatapb/mock"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
)

type fakeAgentsInfo struct {
	wg *sync.WaitGroup
	t  *testing.T
}

// ClearPendingState implementation of clear state for fake agents info.
func (a *fakeAgentsInfo) ClearPendingState() {
}

// DistributedState implementation for fake agents info.
func (a *fakeAgentsInfo) DistributedState() distributedpb.DistributedState {
	return distributedpb.DistributedState{}
}

func (a *fakeAgentsInfo) UpdateAgentsInfo(update *metadatapb.AgentUpdatesResponse) error {
	if len(update.AgentUpdates) > 0 || len(update.AgentSchemas) > 0 {
		a.wg.Done()
	}
	return nil
}

func TestAgentsInfo_GetAgentInfo(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// This test tries out various agent state updates together and in a row to make sure
	// that they all interact with each other properly.
	testSchema := makeTestSchema(t)
	uuidpbs := makeTestAgentIDs(t)

	agents := makeTestAgents(t)
	agentDataInfos := makeTestAgentDataInfo()

	msg1 := &metadatapb.AgentUpdatesResponse{
		AgentUpdates: []*metadatapb.AgentUpdate{
			{
				AgentID: uuidpbs[0],
				Update: &metadatapb.AgentUpdate_Agent{
					Agent: agents[0],
				},
			},
			{
				AgentID: uuidpbs[0],
				Update: &metadatapb.AgentUpdate_DataInfo{
					DataInfo: agentDataInfos[0],
				},
			},
		},
		AgentSchemas: nil,
	}
	msg2 := &metadatapb.AgentUpdatesResponse{
		AgentUpdates: nil,
		AgentSchemas: testSchema,
	}

	mockClient := mock_metadatapb.NewMockMetadataServiceClient(ctrl)
	mockResp := mock_metadatapb.NewMockMetadataService_GetAgentUpdatesClient(ctrl)
	mockResp.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	mockResp.EXPECT().Recv().Return(msg1, nil)
	mockResp.EXPECT().Recv().Return(msg2, nil)
	mockResp.EXPECT().Recv().Return(&metadatapb.AgentUpdatesResponse{}, nil).AnyTimes()

	mockClient.EXPECT().
		GetAgentUpdates(gomock.Any(), gomock.Any()).
		Return(mockResp, nil)

	var wg sync.WaitGroup
	agentsInfo := &fakeAgentsInfo{&wg, t}
	agentsTracker := tracker.NewAgentsWithInfo(mockClient, "fakesigningkey", agentsInfo)
	agentsTracker.Start()
	// 2 sets of updates
	wg.Add(2)

	wg.Wait()
	agentsTracker.Stop()
}
