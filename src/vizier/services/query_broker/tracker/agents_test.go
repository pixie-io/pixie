package tracker_test

import (
	"sync"
	"testing"

	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"

	distributedpb "pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	utils "pixielabs.ai/pixielabs/src/utils"
	testingutils "pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	mock_metadatapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"
)

type fakeAgentsInfo struct {
	wg *sync.WaitGroup
	t  *testing.T
}

// ClearState implementation of clear state for fake agents info.
func (a *fakeAgentsInfo) ClearState() {
}

// DistributedState implementation for fake agents info.
func (a *fakeAgentsInfo) DistributedState() *distributedpb.DistributedState {
	return nil
}

func (a *fakeAgentsInfo) UpdateAgentsInfo(agentUpdates []*metadatapb.AgentUpdate, schemaInfos []*distributedpb.SchemaInfo,
	updatedSchema bool) error {
	if len(agentUpdates) > 0 || len(schemaInfos) > 0 {
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
	var uuids []uuid.UUID
	for _, uuidpb := range uuidpbs {
		uuids = append(uuids, utils.UUIDFromProtoOrNil(uuidpb))
	}

	agents := makeTestAgents(t)
	agentDataInfos := makeTestAgentDataInfo()

	mockClient := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	var wg sync.WaitGroup
	agentsInfo := &fakeAgentsInfo{&wg, t}
	agentsTracker := tracker.NewAgentsWithInfo(mockClient, "fakesigningkey", agentsInfo)
	agentsTracker.Start()
	// 2 sets of updates
	wg.Add(2)

	mockResp := mock_metadatapb.NewMockMetadataService_GetAgentUpdatesClient(ctrl)

	msg1 := &metadatapb.AgentUpdatesResponse{
		AgentUpdates: []*metadatapb.AgentUpdate{
			&metadatapb.AgentUpdate{
				AgentID: uuidpbs[0],
				Update: &metadatapb.AgentUpdate_Agent{
					Agent: agents[0],
				},
			},
			&metadatapb.AgentUpdate{
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

	mockResp.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	mockResp.EXPECT().Recv().Return(msg1, nil)
	mockResp.EXPECT().Recv().Return(msg2, nil)
	mockResp.EXPECT().Recv().Return(&metadatapb.AgentUpdatesResponse{}, nil).AnyTimes()

	mockClient.EXPECT().
		GetAgentUpdates(gomock.Any(), gomock.Any()).
		Return(mockResp, nil)

	wg.Wait()
	agentsTracker.Stop()
}
